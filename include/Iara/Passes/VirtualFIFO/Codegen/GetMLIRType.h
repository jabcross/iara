
#ifndef IARA_PASSES_VIRTUALFIFO_CODEGEN_GETMLIRTYPE_H
#define IARA_PASSES_VIRTUALFIFO_CODEGEN_GETMLIRTYPE_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/Span.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include <boost/pfr/core.hpp>
#include <cerrno>
#include <cstdio>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Support/TypeID.h>
#include <tuple>
#include <type_traits>

namespace iara::passes::virtualfifo::codegen {
using namespace mlir;

extern inline std::map<size_t, Type> &memo() {
  static std::map<size_t, Type> memo{};
  return memo;
};

template <typename T> struct GetMLIRType {};

template <class T> inline auto getMLIRType(MLIRContext *context) {
  return GetMLIRType<T>::get(context);
}

template <typename T> struct GetMLIRType<T &> {
  static Type get(MLIRContext *context) { return getMLIRType<T>(context); }
};

template <typename T> struct GetMLIRType<const T> {
  static Type get(MLIRContext *context) { return getMLIRType<T>(context); }
};

template <typename T> struct GetMLIRType<T const *> {
  static Type get(MLIRContext *context) { return getMLIRType<T *>(context); }
};

template <typename T> struct GetMLIRType<std::vector<T>> {
  static Type get(MLIRContext *context) {
    return getMLIRType<Span<T>>(context);
  }
};

template <typename T> struct GetMLIRType<std::vector<T> *> {
  static Type get(MLIRContext *context) {
    return getMLIRType<Span<T>>(context);
  }
};

template <class... Args> struct tag {};

template <class T, class... Args>
inline void
fillWithTypes(MLIRContext *context, std::vector<Type> &types, tag<T, Args...>) {
  types.push_back(getMLIRType<T>(context));
  fillWithTypes(context, types, tag<Args...>{});
}

inline void
fillWithTypes(MLIRContext *context, std::vector<Type> &types, tag<>) {}

template <class... Args> auto getTypes(MLIRContext *context) {
  std::vector<Type> types;
  fillWithTypes(context, types, tag<Args...>{});
  return types;
}

template <class... Args> struct GetTypes {
  static auto get(MLIRContext *ctx) { return getTypes<Args...>(ctx); }
};

template <typename R, typename... Args> struct GetMLIRType<R(Args...)> {
  static Type get(MLIRContext *context) {
    std::vector<Type> arg_types = getTypes<Args...>(context);
    return LLVM::LLVMFunctionType::get(getMLIRType<R>(context), arg_types);
  }
};

// template <typename T>
// concept is_llvm_struct = requires(T a, MLIRContext *ctx) {
//   { static_cast<LLVM::LLVMStructType>(getMLIRType<T>(ctx)) };
// };

template <typename T> struct GetMLIRType<T *> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

// template <typename T>
//   requires is_llvm_struct<T>
// struct GetMLIRType<T *> {
//   static Type get(MLIRContext *context) {
//     return LLVM::LLVMPointerType::get(
//         cast<LLVM::LLVMStructType>(getMLIRType<T>(context)));
//   }
// };

// template <> struct GetMLIRType<void *> {
//   static Type get(MLIRContext *context) {
//     return LLVM::LLVMPointerType::get(context);
//   }
// };

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
  static Type get(MLIRContext *context) { return FloatType::getF32(context); }
};

template <> struct GetMLIRType<double> {
  static_assert(sizeof(double) == 8);
  static Type get(MLIRContext *context) { return FloatType::getF64(context); }
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

template <> struct GetMLIRType<LLVM::GlobalOp> {
  static LLVM::LLVMPointerType get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <> struct GetMLIRType<SymbolOpInterface> {
  static LLVM::LLVMPointerType get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <class... Args>
auto fill(MLIRContext *context, std::tuple<Args...> *) {
  return getTypes<Args...>(context);
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

template <class T>
  requires std::is_class_v<T>
struct GetMLIRType<T> {
  static LLVM::LLVMStructType get(MLIRContext *context) {
    // auto _struct =
    //     LLVM::LLVMStructType::getIdentified(context, typeid(T).name());

    // if (!_struct.getBody().empty()) {
    //   return _struct;
    // }

    using Tuple = decltype(boost::pfr::structure_to_tuple(std::declval<T>()));

    std::vector<Type> types = fill(context, (Tuple *)nullptr);

    assert(types.size() == std::tuple_size_v<Tuple>);

    for (auto type : types) {
      assert(LLVM::isCompatibleType(type));
    }

    // auto success = _struct.setBody(types, false);
    auto _struct = LLVM::LLVMStructType::getLiteral(context, types);
    // assert(success.succeeded());
    return _struct;
  }
};

} // namespace iara::passes::virtualfifo::codegen
#endif
