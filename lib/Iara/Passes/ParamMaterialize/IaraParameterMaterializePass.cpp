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
#include "Iara/Dialect/Broadcast.h"
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
#include <mlir/IR/PatternMatch.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
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

// Auto-inserted broadcast/borrow/join nodes (canonicalize's fan-out expansion)
// carry placeholder dynamic result types mirroring their input 1:1: a copy
// broadcast's every output aliases the whole input, a borrow's passthrough +
// aliases are the input type, and a join's result (when present) matches the
// broadcast it pairs with. Every dynamic input is resolved by the manual loops
// in resolveTypes, so this pattern only needs a static first input — applied
// greedily so producer ordering never matters.
struct ResolvePlaceholderBroadcastPattern
    : public mlir::OpRewritePattern<NodeOp> {
  using mlir::OpRewritePattern<NodeOp>::OpRewritePattern;

  static bool isBcastShaped(NodeOp node) {
    llvm::StringRef impl = node.getImpl();
    return impl.starts_with("iara_broadcast") ||
           impl.starts_with("iara_bcast_borrow") || impl.starts_with("iara_join");
  }

  mlir::LogicalResult matchAndRewrite(
      NodeOp node, mlir::PatternRewriter &rewriter) const override {
    if (!node.getOutDynamicSizes().empty())
      return mlir::failure(); // declared sizes — resolved by the manual loop
    if (!isBcastShaped(node))
      return mlir::failure();
    bool hasDynamic = false;
    for (auto r : node.getResults())
      if (auto t = llvm::dyn_cast<mlir::RankedTensorType>(r.getType()))
        hasDynamic |= !t.hasStaticShape();
    if (!hasDynamic)
      return mlir::failure();
    auto input = node.getAllInputs().front();
    auto inputTy = llvm::dyn_cast<mlir::RankedTensorType>(input.getType());
    if (!inputTy || !inputTy.hasStaticShape())
      return mlir::failure(); // input unresolved yet — re-matches once resolved
    llvm::SmallVector<mlir::Type> newResultTypes;
    for (auto r : node.getResults()) {
      auto t = llvm::dyn_cast<mlir::RankedTensorType>(r.getType());
      if (t && !t.hasStaticShape())
        newResultTypes.push_back(mlir::Type(inputTy));
      else
        newResultTypes.push_back(r.getType());
    }
    auto rep = rewriter.create<NodeOp>(node.getLoc(), newResultTypes,
                                       node.getImpl(), node.getParams(),
                                       node.getIn(), node.getInout());
    rep->setDiscardableAttrs(node->getDiscardableAttrDictionary());
    rewriter.replaceOp(node, rep.getResults());
    return mlir::success();
  }
};

// An EdgeOp whose `out` is dynamic and whose `out_static_sizes` is empty
// mirrors its `in` (no independent sizing of its own — e.g. the edges
// Canonicalize's expandImplicitEdge wraps around a fan-out broadcast). The
// declared multi-rate case (non-empty out_static_sizes, resolved from folded
// out_dynamic_sizes) is NOT handled here. Greedy application resolves the
// mirror ordering naturally: a broadcast's input edge mirrors first (its `in`
// is the already-resolved producer), the broadcast then staticizes, and its
// output edges mirror last — fixpoint, no manual pass ordering.
struct MirrorEdgePattern : public mlir::OpRewritePattern<EdgeOp> {
  using mlir::OpRewritePattern<EdgeOp>::OpRewritePattern;

  mlir::LogicalResult matchAndRewrite(
      EdgeOp edge, mlir::PatternRewriter &rewriter) const override {
    auto outTy = llvm::dyn_cast<mlir::RankedTensorType>(edge.getOut().getType());
    if (!outTy || outTy.hasStaticShape())
      return mlir::failure();
    if (!edge.getOutStaticSizes().empty())
      return mlir::failure(); // declared multi-rate — resolved by the manual loop
    auto inTy = llvm::dyn_cast<mlir::RankedTensorType>(edge.getIn().getType());
    if (!inTy || !inTy.hasStaticShape())
      return mlir::failure(); // `in` unresolved yet — re-matches once resolved
    auto rep = rewriter.create<EdgeOp>(edge.getLoc(), inTy, edge.getIn());
    rep->setDiscardableAttrs(edge->getDiscardableAttrDictionary());
    rewriter.replaceOp(edge, rep.getOut());
    return mlir::success();
  }
};

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
    // NodeOps: staticize the dynamic dims of the output tensors from the
    // folded out_dynamic_sizes (result-then-dim order, matching kDynamic
    // slots in out_static_sizes), then rebuild the node with static result
    // types and no size operands. Runs before the EdgeOp pass so edges pick
    // up the now-static producer type. A node with no kDynamic slots at all
    // (the common case) is left untouched.
    for (auto node : actor.getOps<NodeOp>() | IntoVector()) {
      if (node.getOutDynamicSizes().empty())
        continue;
      llvm::SmallVector<int64_t> sizes;
      for (auto s : node.getOutDynamicSizes())
        sizes.push_back(constIntOf(s, "NodeOp out_dynamic_size"));
      unsigned si = 0;
      llvm::SmallVector<mlir::Type> newResultTypes;
      for (auto r : node.getResults()) {
        auto ty = llvm::dyn_cast<mlir::RankedTensorType>(r.getType());
        if (!ty || ty.hasStaticShape()) {
          newResultTypes.push_back(r.getType());
          continue;
        }
        unsigned nd = 0;
        for (auto d : ty.getShape())
          if (mlir::ShapedType::isDynamic(d))
            nd++;
        assert(si + nd <= sizes.size() &&
               "out_dynamic_sizes fewer than dynamic dims");
        newResultTypes.push_back(
            staticize(ty, llvm::ArrayRef<int64_t>(sizes).slice(si, nd)));
        si += nd;
      }
      assert(si == sizes.size() &&
             "out_dynamic_sizes count != total dynamic dims");
      mlir::OpBuilder b(node);
      auto rep = b.create<NodeOp>(node.getLoc(), newResultTypes, node.getImpl(),
                                  node.getParams(), node.getIn(),
                                  node.getInout());
      rep->setDiscardableAttrs(node->getDiscardableAttrDictionary());
      for (unsigned i = 0; i < node.getNumResults(); ++i)
        node.getResult(i).replaceAllUsesWith(rep.getResult(i));
      node.erase();
    }
    // EdgeOps: an edge whose `out` has no dynamic dims (static tensor as
    // authored, or a non-tensor type like !Config/i32) is left completely
    // untouched — covers genuinely single-rate edges (out already static)
    // and genuine multi-rate edges whose out_static_sizes fully resolved
    // already. For a dynamic `out`, two distinct cases:
    //  - out_static_sizes is non-empty (a real declaration, e.g. degridder's
    //    NUM_CHUNK-driven chunking): resolve independently from
    //    out_dynamic_sizes, never from `in` — this is the actual multi-rate
    //    case, where in/out legitimately differ.
    //  - out_static_sizes is empty (no declaration at all — e.g.
    //    Canonicalize's expandImplicitEdge, which mechanically wraps a value
    //    in an edge with no independent sizing of its own): mirror `in`'s
    //    type, which is already resolved by this point (InPortOp/NodeOp
    //    loops above run first).
    for (auto edge : actor.getOps<EdgeOp>() | IntoVector()) {
      auto outTy = llvm::dyn_cast<mlir::RankedTensorType>(edge.getOut().getType());
      if (!outTy || outTy.hasStaticShape())
        continue;
      mlir::RankedTensorType newTy;
      if (edge.getOutStaticSizes().empty()) {
        newTy = llvm::dyn_cast<mlir::RankedTensorType>(edge.getIn().getType());
        if (!newTy || !newTy.hasStaticShape())
          continue; // `in` not resolved yet either; a later pass or the
                    // loud guard below will catch a genuinely stuck case.
      } else {
        llvm::SmallVector<int64_t> dims;
        for (auto d : edge.getOutDynamicSizes())
          dims.push_back(constIntOf(d, "EdgeOp out_dynamic_size"));
        newTy = staticize(outTy, dims);
      }
      mlir::OpBuilder b(edge);
      auto rep = b.create<EdgeOp>(edge.getLoc(), newTy, edge.getIn());
      rep->setDiscardableAttrs(edge->getDiscardableAttrDictionary());
      edge.getOut().replaceAllUsesWith(rep.getOut());
      edge.erase();
    }
    // Resolve placeholder broadcast/borrow/join result types and the edges
    // around them (ResolvePlaceholderBroadcastPattern + MirrorEdgePattern).
    // All dynamic inputs are static by now; greedy application resolves the
    // broadcast-then-edges ordering as a fixpoint rather than hand-ordering it.
    mlir::RewritePatternSet patterns(actor.getContext());
    patterns.add<ResolvePlaceholderBroadcastPattern>(actor.getContext());
    patterns.add<MirrorEdgePattern>(actor.getContext());
    mlir::FrozenRewritePatternSet frozen(std::move(patterns));
    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(actor.getBody(),
                                                        frozen)))
      return signalPassFailure();

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
                      mlir::arith::DivSIOp, mlir::arith::DivUIOp,
                      mlir::arith::IndexCastOp>(&op) &&
            op.use_empty()) {
          op.erase();
          changed = true;
        }
      }
    }

    // Broadcasts whose types were dynamic at canonicalize time kept their
    // placeholder impl (no size computable then). Recompute the real name from
    // the now-static types and codegen the copy impl. Static broadcasts (e.g.
    // SIFT's explicit ones) recompute to the same name — idempotent.
    for (auto node : actor.getOps<NodeOp>() | IntoVector()) {
      if (!node.getImpl().starts_with("iara_broadcast"))
        continue;
      iara::dialect::broadcast::specializeBroadcast(node, false);
      iara::dialect::broadcast::getOrCodegenBroadcastImpl(node);
    }

    // Safety net: any node/edge result still dynamic here means its author
    // forgot to declare out_dynamic_sizes/out_static_sizes for that dim (or
    // the sizes provided were incomplete) — fail loudly rather than emit a
    // topology with a stray `?` that later passes would silently mishandle.
    actor.walk([&](NodeOp node) {
      for (auto r : node.getResults())
        if (auto t = llvm::dyn_cast<mlir::RankedTensorType>(r.getType()))
          if (!t.hasStaticShape()) {
            node.emitError("iara-param-materialize: node result has unresolved "
                           "dynamic type; missing out_dynamic_sizes/"
                           "out_static_sizes for this dimension");
            signalPassFailure();
          }
    });
    actor.walk([&](EdgeOp edge) {
      if (auto t = llvm::dyn_cast<mlir::RankedTensorType>(edge.getOut().getType()))
        if (!t.hasStaticShape()) {
          edge.emitError("iara-param-materialize: edge result has unresolved "
                         "dynamic type; missing out_dynamic_sizes/"
                         "out_static_sizes for this dimension");
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
