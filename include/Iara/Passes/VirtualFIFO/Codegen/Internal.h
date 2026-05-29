#ifndef IARA_PASSES_VIRTUALFIFO_CODEGEN_INTERNAL_H
#define IARA_PASSES_VIRTUALFIFO_CODEGEN_INTERNAL_H

#include "Iara/Passes/Common/Codegen/AsValue.h"
#include "Iara/Passes/Common/Codegen/Codegen.h"
#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/OpCreateHelper.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/Value.h>

namespace iara::passes::virtualfifo::codegen {
using namespace mlir;
using namespace mlir::LLVM;

inline Value createConstOp(OpBuilder builder, Location loc, auto val) {
  DEF_OP(Value,
         _const,
         ConstantOp,
         builder,
         loc,
         iara::passes::common::codegen::getMLIRType<decltype(val)>(
             builder.getContext()),
         val);
  return _const;
}

inline Value getNullPtr(OpBuilder builder, Location loc) {
  return CREATE(
      ZeroOp, builder, loc, LLVM::LLVMPointerType::get(builder.getContext()));
}

inline Value getEmptySpan(OpBuilder builder, Location loc) {
  Value _struct = CREATE(UndefOp,
                         builder,
                         loc,
                         iara::passes::common::codegen::getSpanType(
                             builder.getContext()));
  _struct = CREATE(InsertValueOp,
                   builder,
                   loc,
                   _struct,
                   getNullPtr(builder, loc),
                   ArrayRef<i64>{0});
  _struct = CREATE(InsertValueOp,
                   builder,
                   loc,
                   _struct,
                   createConstOp(builder, loc, i64(0)),
                   ArrayRef<i64>{1});
  return _struct;
}

} // namespace iara::passes::virtualfifo::codegen

#endif
