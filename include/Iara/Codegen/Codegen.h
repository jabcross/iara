#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Iara/Codegen/GetMLIRType.h"
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
  auto val = asValue(global_builder, loc, e);
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
  auto elem_type = getMLIRType<decltype(*range.begin())>(module.getContext());
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
      CREATE(LLVM::LLVMFuncOp, builder, module.getLoc(), func_name, func_type);
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

// We don't know that Attributes are stored as contiguous arrays, so we need to
// do this witchcraft.
// (i64 for alignment reasons)

// inline std::vector<i64> convertToVecOfByte(DenseArrayAttr dense) {
//   struct Result {
//     Type type;
//     void *buffer;
//     i64 size;
//   } rv;
//   i64 num_valid = 0;
//   forEachType(SupportedDataTypes{}, [&](auto x) {
//     using TheType = decltype(x)::type;
//     auto type = getMLIRType<TheType>(dense.getContext());
//     if (dense.getElementType() == type) {
//       num_valid++;
//       auto buf = dense.getRawData();
//       dense.getRawData();

//       DenseElementsAttr::auto range = dense.getValues<TheType>();

//       std::vector<TheType> buffer;
//       buffer.reserve(dense.size());
//       for (auto element : range) {
//         buffer.push_back(element);
//       }

//       buffer.
//     }
//   });
//   assert(num_valid == 1);
//   return rv;
// }

} // namespace iara::codegen
#endif
