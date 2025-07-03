#ifndef IARA_DIALECT_BROADCAST_H
#define IARA_DIALECT_BROADCAST_H

#include "Iara/Util/CommonTypes.h"
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Types.h>

namespace iara::dialect::broadcast {

mlir::LLVM::LLVMFuncOp
getOrCodegenBroadcastImpl(mlir::Value value, i64 size, bool reuse_first);

mlir::LogicalResult expandToBroadcast(mlir::OpResult &value);

} // namespace iara::dialect::broadcast
#endif