#ifndef IARA_PASSES_VIRTUALFIFO_SDF_H
#define IARA_PASSES_VIRTUALFIFO_SDF_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CompilerTypes.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Operation.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::ringbuffer::sdf {

using namespace dialect;

struct StaticAnalysisData {
  DenseMap<EdgeOp, RingBuffer_Edge::StaticInfo> edge_static_info;
  DenseMap<NodeOp, RingBuffer_Node::StaticInfo> node_static_info;
};

FailureOr<StaticAnalysisData> analyzeAndAnnotate(ActorOp actor);
LogicalResult generateAllocsAndFrees(ActorOp actor, StaticAnalysisData &data);

} // namespace iara::passes::ringbuffer::sdf

#endif // IARA_UTIL_SDF_H
