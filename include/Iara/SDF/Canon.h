#ifndef UTIL_CANON_H
#define UTIL_CANON_H

#include "Iara/Dialect/IaraOps.h"
#include <mlir/Support/LogicalResult.h>
namespace iara::sdf::canon {

LogicalResult expandImplicitEdgesAndBroadcasts(ActorOp actor);
LogicalResult canonicalizeTypes(ActorOp actor);
func::FuncOp codegenBroadcastImpl(Value value, i64 size);
LogicalResult expandToBroadcast(OpResult &value);

// Expands implicit edges, normalizes types, generates broadcasts
LogicalResult canonicalize(ActorOp actor);

} // namespace iara::sdf::canon

#endif // UTIL_CANON_H
