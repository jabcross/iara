#include "Iara/Dialect/Broadcast.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/EnvOption.h"
#include "Iara/Util/OpCreateHelper.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>

namespace iara::passes::virtualfifo {

// Ping-pong feedback (IARA_DELAY_PINGPONG=1): under data-triggered alloc every
// firing already gets a distinct per-block malloc (makeAllocChunkForFiring), so
// a feedback consumer reading block N-1 while the producer writes block N never
// aliases the live write — the copy breakEdge would insert is redundant. Skip
// it: the delay edge alone breaks the SDF cycle (initial tokens), and the
// per-block buffers ARE the ping-pong. Only under data-triggered alloc (priming
// mode has no per-block malloc, so in-place would genuinely alias).
static bool pingpongDelayEnabled() {
  return iara::util::optionOrEnv(false, "", "IARA_DELAY_PINGPONG", "0") == "1" &&
         iara::util::optionOrEnv(false, "", "IARA_ALLOC_MODE", "") ==
             "data-triggered";
}

// insert a copy.
void breakEdge(EdgeOp edge) {
  if (pingpongDelayEnabled() && edge->hasAttr("delay"))
    return;
  auto builder = OpBuilder(edge);
  auto bc = broadcast::insertBroadcast(edge.getIn(), true);
  LLVM::LLVMFuncOp impl = broadcast::getOrCodegenBroadcastImpl(bc);
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
  iara::passes::canonicalize::expandImplicitEdge(copy_op->getOperand(0));
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