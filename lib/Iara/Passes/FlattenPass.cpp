//===- IaraPasses.cpp - Iara passes -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
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
#include <mlir/IR/Value.h>
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

  DenseSet<ActorOp> m_top_level_candidates;

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

  // If successful, node will be erased. Leaves double edges on interface
  // boundaries.
  void flatten(ModuleOp module, NodeOp node) {
    if (node->hasAttr("flat")) {
      return;
    }
    auto impl = node.getImpl();
    auto impl_actor_op = module.lookupSymbol<ActorOp>(impl);
    if (!impl_actor_op) {
      return;
    }
    // If a node is referring to it, it can't be the top level, can it?
    m_top_level_candidates.erase(impl_actor_op);
    if (impl_actor_op.isKernelDeclaration()) {
      return;
    }
    if (!node.signatureMatches(impl_actor_op)) {
      node->emitError("Signature of node does not match actor");
      for (auto [i, n, a] :
           enumerate(node.getAllInputs(), impl_actor_op.getOps<InPortOp>())) {
        auto n_t = n.getType();
        auto a_t = a.getResult().getType();
        if (n_t != a_t) {
          llvm::errs() << "Node input #" << i << " of type " << n_t
                       << " != actor input of type " << a_t << "\n";
        }
      }
      for (auto [i, n, a] :
           enumerate(node.getAllOutputs(), impl_actor_op.getOps<OutPortOp>())) {
        auto n_t = n.getType();
        auto a_t = a.getValue().getType();
        if (n_t != a_t) {
          llvm::errs() << "Node output #" << i << " of type " << n_t
                       << " != actor output of type " << a_t << "\n";
        }
      }
      llvm::errs() << "Node: ";
      node.dump();
      llvm::errs() << "Actor: ";
      impl_actor_op.dump();
      signalPassFailure();
      llvm_unreachable("Error");
      return;
    }
    if (impl_actor_op.getOps<NodeOp>().empty()) {
      node->emitError("Actor does not have any nodes (is this a declaration?)");
      signalPassFailure();
      llvm_unreachable("should have been caught by signature check");
      return;
    }

    auto next_op = node->getNextNode();

    OpBuilder builder(node);
    ActorOp new_actor{impl_actor_op->clone()};
    assert(new_actor->getParentOp() == nullptr);

    // Replace callee block args with caller param operands.
    auto &callee_block = new_actor.getBody().front();
    size_t num_params = node.getParams().size();
    if (num_params > 0) {
      assert(num_params <= callee_block.getNumArguments() &&
             "Callee block arg count insufficient for caller param count");
      for (auto [callee_arg, caller_param] :
           llvm::zip(callee_block.getArguments().take_front(num_params),
                     node.getParams())) {
        callee_arg.replaceAllUsesWith(caller_param);
      }
      callee_block.eraseArguments(0, num_params);
    }

    SmallVector<InPortOp> in_ops{new_actor.getOps<InPortOp>()};

    // Build the in+inout value list (excluding params) for matching.
    SmallVector<Value> node_in_inout;
    for (auto v : node.getIn())
      node_in_inout.push_back(v);
    for (auto v : node.getInout())
      node_in_inout.push_back(v);

    if (in_ops.size() != node_in_inout.size()) {
      node.emitError(
          "Mismatch in number of node in+inout and actor InPortOps (")
          << node_in_inout.size() << " vs " << in_ops.size() << ")";
      llvm::errs() << "Actor:\n";
      new_actor.dump();
      llvm::errs() << "Node:\n";
      node.dump();
      llvm_unreachable("Error");
      return;
    }

    for (auto [inner_input_port_op, outer_input_value] :
         llvm::zip(in_ops, node_in_inout)) {

      inner_input_port_op.getResult().replaceAllUsesWith(outer_input_value);
      inner_input_port_op->erase();
    }

    SmallVector<OutPortOp> out_ops{new_actor.getOps<OutPortOp>()};

    if (out_ops.size() != node->getNumResults()) {
      node.emitError("Mismatch in number of node results and actor OutPortOps(")
          << node->getNumResults() << " vs " << out_ops.size() << ")";
      llvm::errs() << "Actor:\n";
      new_actor.dump();
      llvm::errs() << "Node:\n";
      node.dump();
      llvm_unreachable("Error");
      return;
    }

    for (auto [inner_output_port, outer_node_result] :
         llvm::to_vector(llvm::zip(out_ops, node.getResults()))) {

      outer_node_result.replaceAllUsesWith(inner_output_port.getValue());
      inner_output_port.erase();
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

  void fixDoubleEdge(EdgeOp edge, DenseSet<EdgeOp> &to_erase) {
    assert(edge.getResult().getNumUses() == 1);
    if (auto next_edge = dyn_cast<EdgeOp>(*edge->getUsers().begin())) {
      auto edge_delay = edge->getAttr("delay");
      auto next_edge_delay = next_edge->getAttr("delay");
      if (edge_delay and next_edge_delay)
        llvm_unreachable("Unimplemented: merge delays");

      // Redirect next_edge's input to skip this edge:  A→B→C  →  A→C.
      // Works uniformly for single-rate and multi-rate combinations.
      if (edge_delay)
        next_edge->setAttr("delay", edge_delay);
      next_edge->setOperand(0, edge.getIn());
      to_erase.insert(edge);
      return fixDoubleEdge(next_edge, to_erase);
    }
  }

  void fixDoubleEdges(ActorOp actor) {
    DenseSet<EdgeOp> to_erase;
    for (auto edge : actor.getOps<EdgeOp>() | IntoVector()) {
      if (isa<EdgeOp>(edge.getIn().getDefiningOp())) {
        // always start on the first edge of the chain
        continue;
      }
      fixDoubleEdge(edge, to_erase);
    }
    for (auto edge : to_erase) {
      edge.erase();
    }
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
      if (actor.isKernelDeclaration()) {
        decls.push_back(actor);
      } else {
        m_top_level_candidates.insert(actor);
        actor_depths.push_back({getDepth(actor), actor});
      }
    }

    llvm::sort(actor_depths);

    for (auto [depth, actor] : actor_depths) {
      flatten(actor);
      fixDoubleEdges(actor);
    }
  }
};

} // namespace
} // namespace iara::passes
