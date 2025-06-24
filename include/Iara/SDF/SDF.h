#ifndef IARA_UTIL_SDF_H
#define IARA_UTIL_SDF_H

#include "Iara/Dialect/IaraOps.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Operation.h>
#include <mlir/Support/LogicalResult.h>
namespace iara::sdf {

using namespace dialect;

bool isDeallocEdge(EdgeOp edge);

Vec<EdgeOp> getInoutChain(EdgeOp edge);

NodeOp findFirstNodeOfChain(EdgeOp edge);

Vec<Vec<EdgeOp>> getInoutChains(ActorOp actor);

struct InoutPair {
  Value in;
  Value out;
};

Vec<InoutPair> getInoutPairs(NodeOp node);

struct SDFNode : NodeOp {
  inline i64 total_firings() {
    auto attr = llvm::dyn_cast_if_present<IntegerAttr>(
        getOperation()->getAttr("total_firings"));
    if (!attr) {
      llvm_unreachable("This node was not processed correctly.");
    }
    return attr.getInt();
  };
};

struct SDFEdge : EdgeOp {
  // SDFEdge(EdgeOp op) {
  //   size_t &_this = *(size_t *)this;
  //   size_t &that = *(size_t *)&op;
  //   _this = that;
  // }
  inline i64 id() { return (i64)(*this)["edge_id"]; }
  inline i64 local_index() { return (i64)(*this)["local_index"]; }
  inline i64 prod_rate() { return (i64)(*this)["prod_rate"]; }
  inline i64 cons_rate() { return (i64)(*this)["cons_rate"]; }
  inline i64 delay_offset() { return (i64)(*this)["delay_offset"]; }
  inline i64 delay_size_bytes() { return (i64)(*this)["delay_size_bytes"]; }
  inline i64 first_chunk_size() { return (i64)(*this)["first_chunk_size"]; }
  inline Attribute delay() { return (*this)["delay"]; }

  // // to use as hashmap key
  // inline bool operator<(SDFEdge const rhs) {
  //   return this->getOperation() < ((SDFEdge)rhs).getOperation();
  // }
};

struct StaticAnalysisData {
  DenseMap<EdgeOp, SDF_OoO_FIFO::StaticInfo> edge_static_info;
  DenseMap<NodeOp, SDF_OoO_Node::StaticInfo> node_static_info;
};

FailureOr<StaticAnalysisData> analyzeAndAnnotate(ActorOp actor);
LogicalResult generateAllocsAndFrees(ActorOp actor, StaticAnalysisData &data);

} // namespace iara::sdf

#endif // IARA_UTIL_SDF_H
