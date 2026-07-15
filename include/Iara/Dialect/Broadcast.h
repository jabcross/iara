#ifndef IARA_DIALECT_BROADCAST_H
#define IARA_DIALECT_BROADCAST_H

#include "Iara/Dialect/IaraOps.h"
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Types.h>
namespace iara::dialect::broadcast {

mlir::LLVM::LLVMFuncOp
getOrCodegenBroadcastImpl(iara::dialect::NodeOp broadcast);

iara::NodeOp insertBroadcast(mlir::Value value, bool force_copy);

iara::NodeOp specializeBroadcast(NodeOp generic_broadcast, bool force_copy);

// True when every output of `broadcast` is borrowed read-only (each consumer
// takes the value in its `in` segment, not `inout`). Gates the FirstKeepsBuffer
// zero-copy path: all-read-only broadcasts can alias one buffer to every reader
// instead of copying. See BroadcastOwnership.h.
bool broadcastOutputsAllReadOnly(iara::dialect::NodeOp broadcast);

} // namespace iara::dialect::broadcast
#endif