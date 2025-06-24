#ifndef IARA_UTIL_SPAN_H
#define IARA_UTIL_SPAN_H

#include <span>
#include <vector>

#ifdef IARA_COMPILER
  #include <mlir/Dialect/LLVMIR/LLVMDialect.h>
  #include <mlir/IR/MLIRContext.h>
  #include <mlir/IR/Types.h>
#endif

template <class T> struct Span {
  T *ptr = nullptr;
  size_t extents = 0;

  operator std::span<T>() const { return std::span<T>{ptr, extents}; }

  static Span<T> from(const std::span<T> &span) {
    if (span.empty())
      return {};
    return {span.begin(), span.size()};
  }

  static Span<T> from(const std::vector<T> &vec) {
    if (vec.empty())
      return {};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return {(T *)&*vec.cbegin(), vec.size()};
#pragma GCC diagnostic pop
  }

  inline size_t size() const { return extents; }

  inline std::span<T> asSpan() { return std::span<T>{ptr, extents}; }
};

#ifdef IARA_COMPILER

inline mlir::Type getSpanType(mlir::MLIRContext *context) {
  return mlir::LLVM::LLVMStructType::getLiteral(
      context,
      {mlir::LLVM::LLVMPointerType ::get(context),
       mlir::IntegerType::get(context, 64)});
}

#endif

#endif