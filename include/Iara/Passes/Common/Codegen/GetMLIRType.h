
#ifndef IARA_PASSES_COMMON_CODEGEN_GETMLIRTYPE_H
#define IARA_PASSES_COMMON_CODEGEN_GETMLIRTYPE_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/ForEachType.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include <boost/pfr/core.hpp>
#include <cerrno>
#include <cstdio>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributeInterfaces.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Support/TypeID.h>
#include <tuple>
#include <type_traits>

namespace iara::passes::common::codegen {
using namespace mlir;

template <typename T> struct GetMLIRType {};

template <class T> inline auto getMLIRType(MLIRContext *context) {
  return GetMLIRType<T>::get(context);
}

template <> struct GetMLIRType<void> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMVoidType::get(context);
  }
};

template <> struct GetMLIRType<i64> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }
};

template <> struct GetMLIRType<int> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, sizeof(int) * 8);
  }
};

template <> struct GetMLIRType<float> {
  static_assert(sizeof(float) == 4);
  static Type get(MLIRContext *context) { return Float32Type::get(context); }
};

template <> struct GetMLIRType<double> {
  static_assert(sizeof(double) == 8);
  static Type get(MLIRContext *context) { return Float64Type::get(context); }
};

template <> struct GetMLIRType<long unsigned int> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }
};

template <> struct GetMLIRType<i8> {
  static Type get(MLIRContext *context) { return IntegerType::get(context, 8); }
};

template <> struct GetMLIRType<char> {
  static Type get(MLIRContext *context) { return IntegerType::get(context, 8); }
};

// All pointers are opaque.
template <typename T> struct GetMLIRType<T *> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

// Spans are a pointer and a size.
template <typename T> struct GetMLIRType<std::span<T>> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMStructType::getLiteral(
        context,
        {LLVM::LLVMPointerType::get(context), IntegerType::get(context, 64)});
  }
};

template <class D> auto to_tuple(D d) {
  auto [... t] = d;
  return std::make_tuple(t...);
}

template <class S> struct TupleType {
  using type = decltype(to_tuple(std::declval<S>()));
};

template <class T>
LLVM::LLVMStructType getIdentifiedLLVMStructTypeFromTupleLike(MLIRContext *ctx,
                                                              StringRef name) {
  auto rv = LLVM::LLVMStructType::getIdentified(ctx, name);
  if (rv.isInitialized())
    return rv;

  using tuple_type = TupleType<T>::type;

  std::vector<Type> types =
      [&]<typename... E>(util::foreachtype::TypeWrapper<std::tuple<E...>>)
      -> std::vector<Type> {
    return {getMLIRType<E>(ctx)...};
  }(util::foreachtype::TypeWrapper<tuple_type>{});

  auto res = rv.setBody(types, false);
  assert(res.succeeded());
  return rv;
};

template <class T>
concept NamedTupleLikeStruct = requires(MLIRContext *ctx, StringRef name) {
  T::STRUCT_NAME;
  getIdentifiedLLVMStructTypeFromTupleLike<T>(ctx, name);
};

template <class T>
  requires NamedTupleLikeStruct<T>
struct GetMLIRType<T> {
  static Type get(MLIRContext *context) {
    return getIdentifiedLLVMStructTypeFromTupleLike<T>(context, T::STRUCT_NAME);
  }
};

template <class T>
void fillWithTypesFromTuple(iara::util::foreachtype::TypeWrapper<T> wrapper,
                            MLIRContext *context,
                            Vec<Type> &vec) {
  using namespace iara::util::foreachtype;
  for_each_tuple_type(wrapper, [&](auto type, auto i) {
    using ElemType = decltype(type)::type;
    vec.push_back(getMLIRType<ElemType>(context));
  });
}

template <class T> auto asAttr(MLIRContext *context, T t);

template <> inline auto asAttr<i64>(MLIRContext *context, i64 value) {
  return IntegerAttr::get(IntegerType::get(context, 64), value);
}

template <> inline auto asAttr<size_t>(MLIRContext *context, size_t value) {
  return IntegerAttr::get(IntegerType::get(context, 64), value);
}

template <> inline auto asAttr<int>(MLIRContext *context, int value) {
  return IntegerAttr::get(IntegerType::get(context, 32), value);
}

template <> inline auto asAttr<float>(MLIRContext *context, float value) {
  return FloatAttr::get(Float32Type::get(context), value);
}

template <> inline auto asAttr<char>(MLIRContext *context, char value) {
  return IntegerAttr::get(IntegerType::get(context, 8), value);
}

} // namespace iara::passes::common::codegen
#endif
