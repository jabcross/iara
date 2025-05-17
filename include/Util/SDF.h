#ifndef IARA_UTIL_SDF_H
#define IARA_UTIL_SDF_H

#include "Iara/IaraOps.h"
#include <mlir/Support/LogicalResult.h>
namespace mlir::iara::sdf {

struct NodeInfo {
  NodeOp op;
  inline i64 total_firings() {
    auto attr =
        llvm::dyn_cast_if_present<IntegerAttr>(op->getAttr("total_firings"));
    if (!attr) {
      llvm_unreachable("This node was not processed correctly.");
    }
    return attr.getInt();
  };

  i64 id() { return (i64)op["node_id"]; }
};

struct EdgeInfo {
  EdgeOp op;
  inline i64 id() { return (i64)op["edge_id"]; }
  inline i64 prod_rate() { return (i64)op["prod_rate"]; }
  inline i64 cons_rate() { return (i64)op["cons_rate"]; }
  inline i64 delay_size_bytes() { return (i64)op["delay_size_bytes"]; }
  inline Attribute delay() { return op["delay"]; }

  // to use as hashmap key
  inline bool operator<(EdgeInfo const &rhs) { return op < rhs.op; }
};

LogicalResult annotateEdgeInfo(ActorOp actor);
LogicalResult analyzeAndAnnotate(ActorOp actor);

} // namespace mlir::iara::sdf

#endif // IARA_UTIL_SDF_H