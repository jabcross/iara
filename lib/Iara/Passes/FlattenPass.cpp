//===- IaraPasses.cpp - Iara passes -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "Iara/Dialect/Canon.h"
#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"
#include "llvm/Support/Casting.h"
#include <cstddef>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/Location.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Support/LLVM.h>

using namespace iara::util::range;

namespace iara::passes {
#define GEN_PASS_DEF_FLATTENPASS
#include "Iara/Dialect/IaraPasses.h.inc"

namespace {

class FlattenPass : public impl::FlattenPassBase<FlattenPass> {
public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect,
                    memref::MemRefDialect,
                    mlir::func::FuncDialect,
                    arith::ArithDialect,
                    func::FuncDialect,
                    LLVM::LLVMDialect,
                    mlir::math::MathDialect,
                    mlir::linalg::LinalgDialect,
                    mlir::tensor::TensorDialect>();
  }
  using impl::FlattenPassBase<FlattenPass>::FlattenPassBase;

  bool hasRecursion() {

    // TODO: dedup with respect to different parameters

    auto module = getOperation();
    DenseSet<ActorOp> visiting;

    std::function<bool(ActorOp)> recurse;
    recurse = [&](ActorOp actor) {
      if (visiting.contains(actor))
        return true;
      visiting.insert(actor);
      for (auto node : actor.getOps<NodeOp>()) {
        auto impl = node.getImplAttr();
        if (auto actor_op = module.lookupSymbol<ActorOp>(impl)) {
          if (recurse(actor_op))
            return true;
        }
      }
      visiting.erase(actor);
      return false;
    };

    for (auto actor : module.getOps<ActorOp>()) {
      if (recurse(actor)) {
        return true;
      }
    }
    return false;
  }

  bool isLeaf(ModuleOp module, ActorOp actor) {
    for (auto node : actor.getOps<NodeOp>()) {
      auto impl = node.getImplAttr();
      if (auto actor_op = module.lookupSymbol<ActorOp>(impl)) {
        return false;
      }
    }
    return true;
  }

  int getDepth(ActorOp actor) {
    int depth = 0;
    for (auto node : actor.getOps<NodeOp>()) {
      auto impl = node.getImplAttr();
      if (auto actor_op =
              actor->getParentOfType<ModuleOp>().lookupSymbol<ActorOp>(impl)) {
        depth = std::max(depth, getDepth(actor_op) + 1);
      }
    }
    return depth;
  }

  void flatten(ActorOp actor) {
    for (auto node : llvm::to_vector(actor.getOps<NodeOp>())) {
      flatten(actor->getParentOfType<ModuleOp>(), node);
    }
    actor->setAttr("flat", OpBuilder(actor).getUnitAttr());
  }

  // If successful, node will be erased.
  void flatten(ModuleOp module, NodeOp node) {
    if (node->hasAttr("flat")) {
      return;
    }
    auto impl = node.getImpl();
    auto actor_op = module.lookupSymbol<ActorOp>(impl);
    if (!actor_op) {
      return;
    }
    if (actor_op.isKernelDeclaration()) {
      return;
    }
    if (!node.signatureMatches(actor_op)) {
      node->emitError("Signature of node does not match actor");
      llvm::errs() << "Node: ";
      node.dump();
      llvm::errs() << "Actor: ";
      actor_op.dump();
      signalPassFailure();
      return;
    }
    if (actor_op.getOps<NodeOp>().empty()) {
      node->emitError("Actor does not have any nodes (is this a declaration?)");
      signalPassFailure();
      llvm_unreachable("should have been caught by signature check");
      return;
    }

    auto next_op = node->getNextNode();

    OpBuilder builder(node);
    ActorOp new_actor{actor_op->clone()};
    assert(new_actor->getParentOp() == nullptr);

    SmallVector<InPortOp> in_ops{new_actor.getOps<InPortOp>()};

    if (in_ops.size() != node->getNumOperands()) {
      node.emitError(
          "Mismatch in number of node parameters and actor InPortOps (")
          << node.getParams().size() << " vs " << in_ops.size() << ")";
      llvm::errs() << "Actor:\n";
      new_actor.dump();
      llvm::errs() << "Node:\n";
      node.dump();
      return;
    }

    for (auto [in_op, in_port] : llvm::zip(in_ops, node.getOperands())) {
      in_op.getResult().replaceAllUsesWith(in_port);
      in_op->erase();
    }

    SmallVector<OutPortOp> out_ops{new_actor.getOps<OutPortOp>()};

    if (out_ops.size() != node->getNumResults()) {
      node.emitError("Mismatch in number of node results and actor OutPortOps(")
          << node->getNumResults() << " vs " << out_ops.size() << ")";
      llvm::errs() << "Actor:\n";
      new_actor.dump();
      llvm::errs() << "Node:\n";
      node.dump();
      return;
    }

    for (auto [out_op, out_port] :
         llvm::to_vector(llvm::zip(out_ops, node.getResults()))) {
      out_port.replaceAllUsesWith(out_op.getValue());
      out_op.erase();
    }

    for (auto node : new_actor.getOps<NodeOp>() | Into<SmallVector<NodeOp>>()) {
      node->moveBefore(next_op);
    }
    for (auto edge : new_actor.getOps<EdgeOp>() | Into<SmallVector<EdgeOp>>()) {
      edge->moveBefore(next_op);
    }

    new_actor.erase();
    node.erase();
  }

  void runOnOperation() final {
    auto module = getOperation();
    if (hasRecursion()) {
      module.emitError(
          "Recursion found in topology (not supported at the moment)");
    }

    llvm::SmallVector<std::pair<int, ActorOp>> actor_depths;

    SmallVector<ActorOp> decls;

    for (auto actor : module.getOps<ActorOp>()) {
      auto _ = canon::canonicalizeTypes(actor);
      if (actor.isKernelDeclaration()) {
        decls.push_back(actor);
      } else {
        actor_depths.push_back({getDepth(actor), actor});
      }
    }

    llvm::sort(actor_depths);

    for (auto [depth, actor] : actor_depths) {
      flatten(actor);
    }

    for (auto actor : decls) {
      actor->erase();
    }
  }
};

} // namespace
} // namespace iara::passes
