#include "Iara/Dialect/IaraOps.h"
#include "Iara/SDF/Canon.h"
#include "Iara/SDF/SDF.h"
#include "Iara/Util/OpCreateHelper.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>

namespace iara::passes::virtualfifo {

// insert a copy.
void breakEdge(EdgeOp edge) {
  auto builder = OpBuilder(edge);
  LLVM::LLVMFuncOp impl =
      iara::sdf::canon::getOrCodegenBroadcastImpl(edge.getIn(), 1, false);
  DEF_OP(NodeOp,
         copy_op,
         NodeOp,
         builder,
         edge->getLoc(),
         {edge.getOut().getType()},
         impl.getSymName(),
         {},
         {edge.getIn()},
         {});
  edge.getIn().replaceAllUsesExcept(copy_op->getResult(0), copy_op);
  edge.dump();
}

void breakLoops(iara::dialect::ActorOp actor) {
  auto chains = sdf::getInoutChains(actor);
  for (auto &chain : chains) {
    DenseSet<NodeOp> visited_nodes;
    DenseSet<EdgeOp> edges_to_break;

    visited_nodes.insert(chain[0].getProducerNode());

    for (auto edge : chain) {
      if (visited_nodes.contains(edge.getConsumerNode())) {
        edges_to_break.insert(edge);
        visited_nodes.insert(edge.getConsumerNode());
      }
    }

    for (auto edge : edges_to_break) {
      breakEdge(edge);
    }
  }
}

} // namespace iara::passes::virtualfifo