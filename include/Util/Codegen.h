#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Util/MlirUtil.h"
#include "Util/OpCreateHelper.h"
#include "Util/types.h"
#include "external/boost/describe.hpp"
#include "mlir/IR/Builders.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/BuiltinTypes.h>

namespace mlir::iara::codegen {
using namespace mlir_util;
template <typename T> struct GetMLIRType {
  static Type get(MLIRContext *context) {
    llvm_unreachable("Unsupported");
    return nullptr;
  }
};

template <typename T> struct GetMLIRType<T &> {
  static Type get(MLIRContext *context) { return GetMLIRType<T>::get(context); }
};

template <> struct GetMLIRType<i64> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }
};

template <> struct GetMLIRType<LLVM::GlobalOp> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <> struct GetMLIRType<SymbolOpInterface> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <typename T> Value asValue(OpBuilder builder, Location loc, T t) {
  return CREATE(LLVM::ConstantOp, builder, loc, asAttr(builder.getContext(), t))
      .getResult();
}

template <>
inline Value asValue<std::nullptr_t>(OpBuilder builder, Location loc,
                                     std::nullptr_t) {
  return CREATE(LLVM::ZeroOp, builder, loc,
                LLVM::LLVMPointerType::get(builder.getContext()))
      .getResult();
}

template <>
inline Value asValue<LLVM::GlobalOp>(OpBuilder builder, Location loc,
                                     LLVM::GlobalOp global_op) {
  if (global_op)
    return CREATE(LLVM::AddressOfOp, builder, loc,
                  LLVM::LLVMPointerType::get(builder.getContext()),
                  global_op.getSymName())
        .getResult();
  else
    return asValue(builder, loc, nullptr);
}

template <>
inline Value asValue<SymbolOpInterface>(OpBuilder builder, Location loc,
                                        SymbolOpInterface op) {
  if (op)
    return CREATE(LLVM::AddressOfOp, builder, loc,
                  LLVM::LLVMPointerType::get(builder.getContext()),
                  op.getName())
        .getResult();
  else
    return asValue(builder, loc, nullptr);
}

} // namespace mlir::iara::codegen
#endif