#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Types.h"
#include "boost/pfr.hpp"
#include "mlir/IR/Builders.h"
#include <bits/types/locale_t.h>
#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <type_traits>

namespace iara::codegen {
using namespace util::mlir;

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
  static LLVM::LLVMPointerType get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <> struct GetMLIRType<SymbolOpInterface> {
  static LLVM::LLVMPointerType get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <class T>
  requires std::is_class_v<T>
struct GetMLIRType<T> {
  static LLVM::LLVMStructType get(MLIRContext *context) {
    Vec<Type> types;
    T *t = nullptr;
    boost::pfr::for_each_field(*t, [&](auto &&member, size_t i) {
      types.push_back(GetMLIRType<decltype(member)>::get(context));
    });
    return LLVM::LLVMStructType::getLiteral(context, types);
  }
};

template <class T> auto getMLIRType(MLIRContext *context) {
  return GetMLIRType<T>::get(context);
}

template <class T> Type getLLVMType(MLIRContext *context) {}

template <class T>
concept ConvertibleToAttr = requires(T t) {
  { asAttr(std::declval<MLIRContext>(), t) };
};

template <class T> struct AsValue {};

template <class T>
  requires ConvertibleToAttr<T>
struct AsValue<T> {
  Value asValue(OpBuilder builder, Location loc, T t) {
    auto rv = CREATE(LLVM::ConstantOp, builder, loc,
                     (asAttr(builder.getContext(), t)));
    return rv.getResult();
  }
};

template <class T>
  requires std::is_class_v<T>
struct AsValue<T> {
  Value asValue(OpBuilder builder, Location loc, T t) {
    auto type = getMLIRType<T>(builder.getContext());
    auto _struct = CREATE(LLVM::UndefOp, builder, loc, type).getResult();
    boost::pfr::for_each_field(t, [&](auto member, size_t i) {
      _struct = CREATE(LLVM::InsertValueOp, builder, loc, _struct,
                       asValue(builder, loc, member), i)
                    .getResult();
    });
    return _struct;
  }
};

template <> struct AsValue<std::nullptr_t> {
  Value asValue(OpBuilder builder, Location loc, std::nullptr_t) {
    return CREATE(LLVM::ZeroOp, builder, loc,
                  cast<Type>(LLVM::LLVMPointerType::get(builder.getContext())))
        .getRes();
  }
};

template <> struct AsValue<LLVM::GlobalOp> {
  Value asValue(OpBuilder builder, Location loc, LLVM::GlobalOp global_op) {
    if (cast<Operation *>(global_op))
      return CREATE(LLVM::AddressOfOp, builder, loc,
                    LLVM::LLVMPointerType::get(builder.getContext()),
                    global_op.getSymName())
          .getResult();
    else
      return asValue(builder, loc, nullptr);
  }
};

template <> struct AsValue<SymbolOpInterface> {
  inline Value asValue(OpBuilder builder, Location loc, SymbolOpInterface op) {
    if (op)
      return CREATE(LLVM::AddressOfOp, builder, loc,
                    LLVM::LLVMPointerType::get(builder.getContext()),
                    op.getName())
          .getResult();
    else
      return asValue(builder, loc, nullptr);
  }
};

template <class T> inline Value asValue(OpBuilder builder, Location loc, T t) {
  return AsValue<T>::asValue(builder, loc, t);
}

template <typename T, typename... Args>
void fillTypeVector(MLIRContext *context, SmallVector<Type> &vec) {
  vec.push_back(GetMLIRType<T>::get(context));
  if constexpr (sizeof...(Args) > 0) {
    fillTypeVector<Args...>(context, vec);
  }
}

// template <typename T> Value asValue(OpBuilder builder, Location loc, T t) {
//   return asValue(builder, loc, t);
// }

template <typename T, typename... Args>
Value fillOutContainer(OpBuilder builder, Value container, i64 position, T t,
                       Args... args) {
  auto loc = container.getDefiningOp()->getLoc();
  auto insert = CREATE(LLVM::InsertValueOp, builder, loc, container,
                       asValue(builder, loc, t), position);
  if constexpr (sizeof...(Args) > 0) {
    return fillOutContainer(builder, insert.getResult(), position + 1, args...);
  }
  return insert.getResult();
}

template <class Element, class Original>
LLVM::GlobalOp createGlobal(OpBuilder builder, Location loc, StringRef name,
                            Element &e, Original &o) {
  auto type = getMLIRType<Element>(builder.getContext());
  LLVM::GlobalOp rv = CREATE(LLVM::GlobalOp, builder, loc, type, false,
                             LLVM::Linkage::External, name, Attribute{});
  rv.getInitializerRegion().emplaceBlock();
  auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
  auto val = asValue(global_builder, loc, t);
  CREATE(LLVM::ReturnOp, global_builder, loc, val);
  return rv;
}

// template <typename... Args>
// LLVM::GlobalOp createGlobalStruct(OpBuilder builder, Location loc,
//                                   StringRef name, Args... args) {
//   auto type = getLLVMStructType<Args...>(builder.getContext());
//   LLVM::GlobalOp rv = CREATE(LLVM::GlobalOp, builder, loc, type, false,
//                              LLVM::Linkage::External, name, Attribute{});
//   rv.getInitializerRegion().emplaceBlock();
//   auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
//   auto _struct = CREATE(LLVM::UndefOp, global_builder, loc,
//   type).getResult(); _struct = fillOutContainer(global_builder, _struct, 0,
//   args...); CREATE(LLVM::ReturnOp, global_builder, loc, _struct); return rv;
// }

inline LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
                                              DenseArrayAttr array,
                                              StringRef name,
                                              bool is_constant) {
  if (array.empty())
    return nullptr;
  auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
  return CREATE(
      LLVM::GlobalOp, mod_builder, loc,
      LLVM::LLVMArrayType::get(array.getElementType(), array.getSize()),
      is_constant, LLVM::Linkage::External, name, array);
};

// Returns null if the range is empty.
template <typename Range>
LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
                                       Range &&range, StringRef name,
                                       bool is_constant) {
  if (range.empty())
    return nullptr;
  auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
  auto elem_type =
      GetMLIRType<decltype(*range.begin())>::get(module.getContext());
  auto items = llvm::to_vector(range);
  auto array_type = LLVM::LLVMArrayType::get(elem_type, items.size());
  LLVM::GlobalOp rv =
      CREATE(LLVM::GlobalOp, mod_builder, loc, array_type, is_constant,
             LLVM::Linkage::External, name, Attribute{});
  rv.getInitializer().emplaceBlock();
  auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
  auto array_value =
      CREATE(LLVM::UndefOp, global_builder, loc, array_type).getResult();
  for (auto [i, v] : llvm::enumerate(items)) {
    array_value = CREATE(LLVM::InsertValueOp, global_builder, loc, array_value,
                         asValue(global_builder, loc, v), i)
                      .getResult();
  }
  CREATE(LLVM::ReturnOp, global_builder, loc, array_value);
  return rv;
};

template <class T, class... Args> struct AsValueVec {
  static Vec<Value> asValueVec(T t, Args... args) {
    Vec<Value> tail = AsValueVec<Args...>::asValueVec(args...);
    tail.insert(0, asValue(t));
    return tail;
  }
};

template <typename FuncType>
LLVM::LLVMFuncOp getLLVMFuncDecl(ModuleOp module, StringRef func_name) {
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());

  if (auto func = module.lookupSymbol<LLVM::LLVMFuncOp>(func_name))
    return func;

  auto func_type = getMLIRType<FuncType>(module.getContext());

  auto rv =
      CREATE(func::FuncOp, builder, module.getLoc(), func_name, func_type);
  rv->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
  rv.setVisibility(mlir::SymbolTable::Visibility::Private);
  return rv;
}

template <class T> struct AsValueVec<T> {
  static Vec<Value> asValueVec(T t) { return {asValue(t)}; }
};
template <class... Args> Vec<Value> asValueVec(Args... args) {
  return AsValueVec<Args...>::asValueVec(args...);
}

template <class T> struct AsStaticGlobal {
  static LLVM::GlobalOp asStaticGlobal(ModuleOp module, StringRef name, T t);
};

template <class T>
  requires std::is_class_v<T>
struct AsStaticGlobal<T> {
  static LLVM::GlobalOp asStaticGlobal(ModuleOp module, Location loc,
                                       StringRef name, T &t) {
    auto module_builder = OpBuilder(module).atBlockBegin(module.getBody());
    DenseMap<Operation *, Operation *> self_referential_pointers;
    auto global = createGlobal(module_builder, loc, name, t, t);
    update_pointers(t, self_referential_pointers);
  }
};

template <class T>
LLVM::GlobalOp asStaticGlobal(ModuleOp module, Location loc, StringRef name,
                              T &t) {
  return AsStaticGlobal<T>::asStaticGlobal(module, name, t);
}

template <class T> void insertIntoVecOfByte(T &t, std::vector<byte> &vec) {
  T *ptr = &t;
  byte *as_bytes = (byte *)ptr;
  for (int i = 0; i < sizeof(T); i++) {
    vec.push_back(as_bytes[i]);
  }
}

template <class T> struct TypeWrapper {
  using type = T;
};

template <class Reduce, class T, class... Tail> struct ForEachType {
  template <class F> static constexpr void apply(F f) {
    f(TypeWrapper<T>{});
    ForEachType<Reduce, Tail...>::apply(f);
  }
};

template <class Reduce, class T, class U> struct ForEachType<Reduce, T, U> {
  template <class F> static constexpr void apply(F f) {
    f(TypeWrapper<T>{});
    f(TypeWrapper<U>{});
  }
};

template <class... Args> struct Holder {};

using SupportedDataTypes = Holder<i64, float>;

template <class... Args, class F>
constexpr auto forEachType(Holder<Args...>, F f) {
  return ForEachType<void, Args...>::apply(f);
}

// We don't know that Attributes are stored as contiguous arrays, so we need to
// do this witchcraft.
inline std::vector<byte> convertToVecOfByte(DenseElementsAttr dense) {
  std::vector<byte> rv;
  int num_valid = 0;
  forEachType(SupportedDataTypes{}, [&](auto x) {
    using TheType = decltype(x)::type;
    auto type = getMLIRType<TheType>(dense.getContext());
    if (dense.getElementType() == type) {
      num_valid++;
      auto range = dense.getValues<TheType>();
      rv.reserve(dense.size() * sizeof(TheType));
      for (auto &i : range) {
        insertIntoVecOfByte(i, rv);
      }
    }
  });
  assert(num_valid == 1);
  return rv;
}

} // namespace iara::codegen
#endif
