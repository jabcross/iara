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

} // namespace iara::dialect::broadcast
#endif