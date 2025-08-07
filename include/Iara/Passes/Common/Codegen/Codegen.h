#ifndef IARA_PASSES_COMMON_CODEGEN_CODEGEN_H
#define IARA_PASSES_COMMON_CODEGEN_CODEGEN_H

#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/MLIRContext.h>

namespace iara::passes::common::codegen {
using namespace mlir;

LLVM::LLVMStructType getSpanType(MLIRContext *ctx);

} // namespace iara::passes::common::codegen

#endif