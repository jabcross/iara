
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/MLIRContext.h>

namespace iara::passes::common::codegen {

using namespace mlir;
LLVM::LLVMStructType getSpanType(MLIRContext *ctx) {
  return LLVM::LLVMStructType::getLiteral(
      ctx,
      {mlir::LLVM::LLVMPointerType::get(ctx), IntegerType::get(ctx, 64)},
      false);
  // auto type = LLVM::LLVMStructType::getIdentified(ctx, "span");
  // if (!type.isInitialized()) {
  //   auto res = type.setBody(
  //       {mlir::LLVM::LLVMPointerType::get(ctx), IntegerType::get(ctx, 64)},
  //       false);
  //   assert(res.succeeded());
  // }

  // return type;
}

} // namespace iara::passes::common::codegen