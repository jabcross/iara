
#ifndef IARA_CODEGEN_GETMLIRTYPE_H
#define IARA_CODEGEN_GETMLIRTYPE_H

#include "Iara/Codegen/Codegen.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include <array>
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
#include <ostream>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
namespace iara::codegen {
using namespace mlir;

extern inline std::map<size_t, Type> &memo() {
  static std::map<size_t, Type> memo{};
  return memo;
};

template <typename T> struct GetMLIRType {};

template <class _T> inline Type getMLIRType(MLIRContext *context) {
  using T = std::remove_reference_t<_T>;
  auto key = typeid(T).hash_code();
  auto entry = memo().find(key);
  if (entry == memo().end()) {
    auto [x, inserted] = memo().try_emplace(key, GetMLIRType<T>::get(context));
    x->second.dump();
    return x->second;
  }

  entry->second.dump();
  return entry->second;
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

template <class... Args> struct tag {};

template <class T, class... Args>
inline void fillWithTypes(MLIRContext *context, Vec<Type> &types,
                          tag<T, Args...>) {
  types.push_back(getMLIRType<T>(context));
  fillWithTypes(context, types, tag<Args...>{});
}

inline void fillWithTypes(MLIRContext *context, Vec<Type> &types, tag<>) {}

template <class... Args> auto getTypes(MLIRContext *context) {
  Vec<Type> types;
  fillWithTypes(context, types, tag<Args...>{});
  return types;
}

template <class... Args> struct GetTypes {
  static auto get(MLIRContext *ctx) { return getTypes<Args...>(ctx); }
};

template <typename R, typename... Args> struct GetMLIRType<R(Args...)> {
  static Type get(MLIRContext *context) {
    Vec<Type> arg_types = getTypes<Args...>(context);
    return LLVM::LLVMFunctionType::get(getMLIRType<R>(context), arg_types);
  }
};

template <typename T> struct GetMLIRType<T *> {
  static Type get(MLIRContext *context) {
    auto ptr_type = LLVM::LLVMPointerType::get(context);
    return ptr_type;
  }
};

template <> struct GetMLIRType<void *> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

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

template <> struct GetMLIRType<long unsigned int> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }
};

template <> struct GetMLIRType<byte> {
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

    Vec<Type> types = fill(context, (Tuple *)nullptr);

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

} // namespace iara::codegen
#endif
