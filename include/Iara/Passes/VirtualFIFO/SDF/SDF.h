#ifndef IARA_PASSES_VIRTUALFIFO_SDF_H
#define IARA_PASSES_VIRTUALFIFO_SDF_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/VirtualFIFO/SDF/BufferSizeCalculator.h"
#include "Iara/Util/CompilerTypes.h"
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Operation.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::virtualfifo::sdf {

using namespace dialect;

bool isDeallocEdge(EdgeOp edge);

Vec<EdgeOp> getInoutChain(EdgeOp edge);

NodeOp findFirstNodeOfChain(EdgeOp edge);

Vec<Vec<EdgeOp>> getInoutChains(ActorOp actor);

struct StaticAnalysisData {
  BufferSizeMemo memo;
};

FailureOr<StaticAnalysisData> analyzeAndAnnotate(ActorOp actor);
LogicalResult generateAllocsAndFrees(ActorOp actor, StaticAnalysisData &data);

} // namespace iara::passes::virtualfifo::sdf

#endif // IARA_UTIL_SDF_H
