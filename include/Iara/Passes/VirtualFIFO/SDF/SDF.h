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

// A logic (control-only) edge is typed `none`: it carries no data buffer, is not
// a kernel argument, and only gates firing (a 1:1 token). It is a normal SDF edge
// for rating/admissibility but is excluded from alloc/dealloc generation, inout
// chains, and kernel-arg accounting.
bool isLogicEdge(EdgeOp edge);

// A borrow edge (tagged `borrow`) is a read-only zero-copy alias produced by an
// all-read-only broadcast: it carries data (a kernel arg for the consumer) but
// owns no buffer — it aliases the broadcast's input, which a join frees once.
// Like a logic edge it belongs to no inout chain and gets no alloc/dealloc; the
// producer pushes the aliased chunk in fireBroadcast() (not the chain walk).
bool isBorrowEdge(EdgeOp edge);

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
