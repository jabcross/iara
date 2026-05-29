#ifndef IARA_PASSES_COMMON_CODEGEN_ASVALUE_H
#define IARA_PASSES_COMMON_CODEGEN_ASVALUE_H

#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/ForEachType.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/Value.h>

namespace iara::passes::common::codegen {
using namespace mlir;

template <class T> struct AsValue {};

template <class T>
  requires NamedTupleLikeStruct<T>
struct AsValue<T> {
  static Value get(OpBuilder builder, Location loc, T &_struct) {
    auto [... elems] = to_tuple(_struct);

    Vec<Value> members = {
        (AsValue<decltype(elems)>::get(builder, loc, elems))...};

    DEF_OP(Value,
           struct_value,
           LLVM::UndefOp,
           builder,
           loc,
           getMLIRType<T>(builder.getContext()));

    size_t counter = 0;
    for (auto member : members) {
      struct_value = CREATE(
          LLVM::InsertValueOp, builder, loc, struct_value, member, counter++);
    }
    return struct_value;
  }
};

inline Value createConstOp(OpBuilder builder, Location loc, auto val) {
  DEF_OP(Value,
         _const,
         arith::ConstantOp,
         builder,
         loc,
         asAttr(builder.getContext(), val));
  return _const;
}

template <class T>
concept AsConstOp = requires(OpBuilder builder, Location loc, T t) {
  asAttr(builder.getContext(), t);
  createConstOp(builder, loc, t);
};

template <class T>
  requires AsConstOp<T>
struct AsValue<T> {
  static Value get(OpBuilder builder, Location loc, auto val) {
    return createConstOp(builder, loc, val);
  }
};

template <class T>
inline Value asValue(OpBuilder builder, Location loc, T value) {
  return AsValue<T>::get(builder, loc, value);
}

inline Value createIdentifiedLLVMStructValue(OpBuilder builder,
                                             Location loc,
                                             StringRef name,
                                             Vec<std::pair<Type, Value>> pairs) {
  auto struct_type =
      LLVM::LLVMStructType::getIdentified(builder.getContext(), name);

  auto types = pairs | util::range::Map([](auto pair) { return pair.first; }) |
               util::range::IntoVector();

  if (!struct_type.isInitialized()) {
    auto res = struct_type.setBody(types, false);
    assert(res.succeeded());
  }

  DEF_OP(Value, struct_value, LLVM::UndefOp, builder, loc, struct_type);

  for (auto [i, pair] : llvm::enumerate(pairs)) {
    auto [type, value] = pair;
    struct_value =
        CREATE(LLVM::InsertValueOp, builder, loc, struct_value, value, i);
  }
  return struct_value;
}

} // namespace iara::passes::common::codegen

#endif
