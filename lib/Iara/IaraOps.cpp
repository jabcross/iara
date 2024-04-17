//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/IaraOps.h"
#include "Iara/IaraDialect.h"
#include "Util/RangeUtil.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Casting.h"
#include <Util/RangeUtil.h>
#include <assert.h>
#include <functional>
#include <iterator>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <mlir/Dialect/Bufferization/IR/BufferizableOpInterface.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LLVM.h>

#define GET_OP_CLASSES
#include "Iara/IaraOps.cpp.inc"

using namespace RangeUtil;

namespace mlir::iara {
bool ActorOp::isFlat() { return (*this)->hasAttr("flat"); }

llvm::SmallVector<Type> ActorOp::getParameterTypes() {
  return getParams() | Map([](auto param) {
           return llvm::cast<TypeAttr>(param).getValue();
         }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getAllInputTypes() {
  return getOps<InPortOp>() |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getPureInTypes() {
  return getOps<InPortOp>() | Filter([](auto op) { return !op.getInout(); }) |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getInoutTypes() {
  return getOps<InPortOp>() | Filter([](auto op) { return op.getInout(); }) |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getPureOutTypes() {
  return getOps<OutPortOp>() | Drop(getInoutTypes().size()) |
         Map([](auto op) { return op.getOperand(0).getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getAllOutputTypes() {
  return getOps<OutPortOp>() | Map([](auto op) { return op.getType(); }) |
         Into<SmallVector<Type>>();
};
bool ActorOp::isInoutInput(size_t idx) {
  return idx >= getPureInTypes().size();
}

mlir::FunctionType ActorOp::getImplFunctionType() {
  OpBuilder builder{*this};
  SmallVector<Type> tensor_types;
  tensor_types.append(getParameterTypes());
  tensor_types.append(getPureInTypes());
  tensor_types.append(getAllOutputTypes());
  assert(llvm::all_of(tensor_types, [](Type t) { return isa<TensorType>(t); }));
  auto memref_types = tensor_types | Map([](auto type) {
                        auto tensor_type = cast<TensorType>(type);
                        return MemRefType::get(tensor_type.getShape(),
                                               tensor_type.getElementType());
                      }) |
                      Into<SmallVector<Type>>();
  return builder.getFunctionType(memref_types, TypeRange{});
}

bool ActorOp::isKernel() { return (*this)->hasAttr("kernel"); }

LogicalResult ActorOp::verifyRegions() {
  int inouts = 0;
  int outs = 0;
  for (auto &op : this->getOps()) {
    if (auto in = dyn_cast<InPortOp>(&op); in && in.getInout())
      inouts++;
    if (isa<OutPortOp>(&op))
      outs++;
  }
  if (inouts > outs)
    return failure();
  return success();
}

bool NodeOp::isInoutInput(OpOperand &operand) {
  for (auto opd : this->getInout()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

bool NodeOp::isPureInInput(OpOperand &operand) {
  for (auto opd : this->getIn()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

::mlir::LogicalResult
NodeOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
  // Todo: better verification
  return success();
}

} // namespace mlir::iara