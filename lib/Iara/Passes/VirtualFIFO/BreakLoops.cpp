#include "Iara/Dialect/Broadcast.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/OpCreateHelper.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>

namespace iara::passes::virtualfifo {

// insert a copy.
void breakEdge(EdgeOp edge) {
  auto builder = OpBuilder(edge);
  LLVM::LLVMFuncOp impl = iara::dialect::broadcast::getOrCodegenBroadcastImpl(
      edge.getIn(), 1, false);
  auto value_in = edge.getIn();
  DEF_OP(NodeOp,
         copy_op,
         NodeOp,
         builder,
         edge->getLoc(),
         {edge.getOut().getType()},
         impl.getSymName(),
         {},
         {value_in},
         {});
  value_in.replaceAllUsesExcept(copy_op->getResult(0), copy_op);
  canonicalize::expandImplicitEdge(copy_op->getOperand(0));
}

void breakLoops(iara::dialect::ActorOp actor) {
  auto chains = sdf::getInoutChains(actor);
  for (auto &chain : chains) {
    DenseSet<NodeOp> visited_nodes;
    DenseSet<EdgeOp> edges_to_break;

    visited_nodes.insert(getProducerNode(chain[0]));

    for (auto edge : chain) {
      if (visited_nodes.contains(getConsumerNode(edge))) {
        edges_to_break.insert(edge);
        visited_nodes.insert(getConsumerNode(edge));
      }
    }

    for (auto edge : edges_to_break) {
      breakEdge(edge);
    }
  }
}

} // namespace iara::passes::virtualfifo