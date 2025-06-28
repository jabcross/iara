#ifndef UTIL_CANON_H
#define UTIL_CANON_H

#include "Iara/Dialect/IaraOps.h"
#include <mlir/Support/LogicalResult.h>

namespace iara::dialect::canon {

LogicalResult canonicalizeTypes(ActorOp actor);

LogicalResult expandImplicitEdgesAndBroadcasts(ActorOp actor);

LLVM::LLVMFuncOp
getOrCodegenBroadcastImpl(Value value, i64 size, bool reuse_first = true);

LogicalResult expandToBroadcast(OpResult &value);

// Expands implicit edges, normalizes types, generates broadcasts
LogicalResult canonicalize(ActorOp actor);

} // namespace iara::dialect::canon

#endif // UTIL_CANON_H
