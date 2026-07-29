//===- IaraParameterMaterializePass.cpp -------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Materialize actor parameters into static tensor types. Runs after --flatten,
// when a single flat graph actor remains (plus leaf kernel declarations). See
// IaraPasses.td for the three-phase description.
//
//===----------------------------------------------------------------------===//
#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/Casting.h"
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/Passes.h>

using namespace iara::util::range;

namespace iara::passes {
#define GEN_PASS_DEF_PARAMETERMATERIALIZEPASS
#include "Iara/Dialect/IaraPasses.h.inc"

namespace {

// Read a folded arith.constant integer value behind an SSA value.
static int64_t constIntOf(mlir::Value v, const char *what) {
  auto c = v.getDefiningOp<mlir::arith::ConstantOp>();
  if (!c) {
    llvm::report_fatal_error(llvm::Twine("iara-param-materialize: ") + what +
                             " did not fold to a constant (missing --sccp fold "
                             "or a non-constant parameter chain)");
  }
  return llvm::cast<mlir::IntegerAttr>(c.getValue()).getInt();
}

// Replace the dynamic dims of `ty` (in order) with `dims`.
static mlir::RankedTensorType staticize(mlir::RankedTensorType ty,
                                        llvm::ArrayRef<int64_t> dims) {
  llvm::SmallVector<int64_t> shape(ty.getShape());
  unsigned di = 0;
  for (auto &d : shape)
    if (mlir::ShapedType::isDynamic(d)) {
      assert(di < dims.size() && "more dynamic dims than dyn_sizes");
      d = dims[di++];
    }
  assert(di == dims.size() && "dyn_sizes count != dynamic dim count");
  return mlir::RankedTensorType::get(shape, ty.getElementType());
}

class ParameterMaterializePass
    : public impl::ParameterMaterializePassBase<ParameterMaterializePass> {
public:
  using impl::ParameterMaterializePassBase<
      ParameterMaterializePass>::ParameterMaterializePassBase;

  // Phase 1: block-arg params -> arith.constant from default_params.
  void materializeParams(ActorOp actor) {
    auto defaults = actor->getAttrOfType<mlir::ArrayAttr>("default_params");
    auto &block = actor.getBody().front();
    unsigned n = block.getNumArguments();
    if (n == 0)
      return;
    if (!defaults || defaults.size() != n) {
      actor.emitError("iara-param-materialize: actor has ")
          << n << " block-arg params but default_params has "
          << (defaults ? defaults.size() : 0) << " entries";
      signalPassFailure();
      m_failed = true;
      return;
    }
    mlir::OpBuilder b(&block, block.begin());
    for (unsigned i = 0; i < n; ++i) {
      auto arg = block.getArgument(i);
      // Each default_params entry is a one-key dict: {name = <int>}.
      auto dict = llvm::cast<mlir::DictionaryAttr>(defaults[i]);
      auto value = llvm::cast<mlir::IntegerAttr>(dict.begin()->getValue());
      auto typed = b.getIntegerAttr(arg.getType(), value.getInt());
      auto cst = b.create<mlir::arith::ConstantOp>(actor.getLoc(), typed);
      arg.replaceAllUsesWith(cst);
    }
    llvm::BitVector all(n, true);
    block.eraseArguments(all);
    actor->removeAttr("default_params");
    actor->removeAttr("params"); // param types no longer describe block args
  }

  // Phase 3: dynamic tensor types -> static, using folded dyn_sizes.
  void resolveTypes(ActorOp actor) {
    // InPortOps: static result type, drop dyn_sizes.
    for (auto in : actor.getOps<InPortOp>() | IntoVector()) {
      if (in.getDynSizes().empty())
        continue;
      llvm::SmallVector<int64_t> dims;
      for (auto d : in.getDynSizes())
        dims.push_back(constIntOf(d, "InPortOp dyn_size"));
      auto ty = llvm::cast<mlir::RankedTensorType>(in.getResult().getType());
      auto newTy = staticize(ty, dims);
      mlir::OpBuilder b(in);
      auto rep = b.create<InPortOp>(in.getLoc(), newTy, mlir::ValueRange{},
                                    in.getInout());
      in.getResult().replaceAllUsesWith(rep.getResult());
      in.erase();
    }
    // EdgeOps: rebuild to match a now-static input.
    for (auto edge : actor.getOps<EdgeOp>() | IntoVector()) {
      if (edge.getIn().getType() == edge.getOut().getType())
        continue;
      mlir::OpBuilder b(edge);
      auto rep = b.create<EdgeOp>(edge.getLoc(), edge.getIn().getType(),
                                  edge.getIn(), edge->getAttrs());
      edge.getOut().replaceAllUsesWith(rep.getOut());
      edge.erase();
    }
    // OutPortOps: drop dyn_sizes (value operand already static via SSA).
    for (auto out : actor.getOps<OutPortOp>() | IntoVector()) {
      if (out.getDynSizes().empty())
        continue;
      mlir::OpBuilder b(out);
      llvm::SmallVector<mlir::Value> operands{out.getValue()};
      b.create<OutPortOp>(out.getLoc(), mlir::TypeRange{}, operands,
                          out->getAttrs());
      out.erase();
    }
    // Drop the now-dead param constants / arith chain (SCCP folds but does not
    // DCE). Only arith.constants and pure arith ops become dead here.
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto &op : llvm::make_early_inc_range(actor.getBody().front())) {
        if (mlir::isa<mlir::arith::ConstantOp, mlir::arith::AddIOp,
                      mlir::arith::MulIOp, mlir::arith::SubIOp,
                      mlir::arith::DivSIOp, mlir::arith::DivUIOp>(&op) &&
            op.use_empty()) {
          op.erase();
          changed = true;
        }
      }
    }

    // ponytail: NodeOp results with dynamic tensor types are not staticized
    // (no size annotation on nodes to resolve them from). Real topologies so
    // far declare node output types statically; fail loudly if that breaks.
    actor.walk([&](NodeOp node) {
      for (auto r : node.getResults())
        if (auto t = llvm::dyn_cast<mlir::RankedTensorType>(r.getType()))
          if (!t.hasStaticShape()) {
            node.emitError("iara-param-materialize: node result has unresolved "
                           "dynamic type; node-output size params not yet "
                           "supported");
            signalPassFailure();
          }
    });
  }

  bool m_failed = false;

  void runOnOperation() final {
    auto module = getOperation();

    // A `default_params` attr marks a top-level actor whose block-arg parameters
    // must be materialized. Sub-graph templates left behind by --flatten keep
    // their block args (their params arrive via caller substitution) but carry
    // no default_params, so they are skipped. Capture the roots up front —
    // Phase 1 strips the attr. If there are none, do nothing (in particular skip
    // Phase 2's SCCP) so param-less graphs, incl. delay cycles, are untouched.
    llvm::SmallVector<ActorOp> roots;
    for (auto actor : module.getOps<ActorOp>())
      if (actor->hasAttr("default_params"))
        roots.push_back(actor);
    if (roots.empty())
      return;

    // Phase 1.
    for (auto actor : roots)
      materializeParams(actor);
    if (m_failed)
      return;

    // Phase 2: fold the arith param chains.
    mlir::OpPassManager pm(mlir::ModuleOp::getOperationName());
    pm.addPass(mlir::createSCCPPass());
    if (failed(runPipeline(pm, module)))
      return signalPassFailure();

    // Phase 3.
    for (auto actor : roots)
      resolveTypes(actor);
  }
};

} // namespace
} // namespace iara::passes
