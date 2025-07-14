#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/Mlir.h"

namespace iara {

using namespace util::range;
using namespace util::mlir;

// Follow an inout edge backwards, or return null.
EdgeOp followInoutChainBackwards(EdgeOp edge) {
  auto prev_node = getProducerNode(edge);
  if (auto in_port = prev_node.getMatchingInoutInput(edge.getIn())) {
    return followChainUntilPrevious<EdgeOp>(in_port);
  }
  return nullptr;
}

// Follow an inout edge forwards, or return null.
EdgeOp followInoutChainForwards(EdgeOp edge) {

  auto next_node = getConsumerNode(edge);
  if (auto out_port = next_node.getMatchingInoutOutput(edge.getOut())) {
    return followChainUntilNext<EdgeOp>(out_port);
  }
  return nullptr;
}

EdgeOp findFirstEdgeOfChain(EdgeOp edge) {
  auto prev = edge;
  while (prev) {
    edge = prev;
    prev = followInoutChainBackwards(prev);
  }
  return edge;
}

EdgeOp findLastEdgeOfChain(EdgeOp edge) {
  auto next = edge;
  while (next) {
    edge = next;
    next = followInoutChainForwards(next);
  }
  return edge;
}

Vec<InoutPair> getInoutPairs(NodeOp node) {
  Vec<InoutPair> rv;
  for (auto [i, o] : llvm::zip_first(node.getInout(), node.getResults())) {
    rv.emplace_back(i, o);
  }
  return rv;
}

i64 getDelaySizeBytes(EdgeOp edge) {
  if (auto attr = edge->getAttr("delay")) {
    if (IntegerAttr int_attr = dyn_cast<IntegerAttr>(attr)) {
      return int_attr.getInt() *
             getTypeSize(getElementTypeOrSelf(edge.getIn().getType()),
                         DataLayout::closest(edge));
    }
    if (DenseArrayAttr d_a_attr = dyn_cast<DenseArrayAttr>(attr)) {
      return d_a_attr.getRawData().size();
    }
    llvm_unreachable("Unknown delay attribute type");
  }
  return 0;
}
}