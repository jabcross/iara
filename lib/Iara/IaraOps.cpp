//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/IaraOps.h"
#include "Iara/IaraDialect.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/Support/LogicalResult.h"
#include "util/util.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Casting.h"
#include <functional>
#include <iterator>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LLVM.h>
#include <ranges>

#define GET_OP_CLASSES
#include "Iara/IaraOps.cpp.inc"

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
  return getOps<OutPortOp>() |
         Map([](auto op) { return op.getOperand(0).getType(); }) |
         Into<SmallVector<Type>>();
};

::mlir::LogicalResult
NodeOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
  // Todo: better verification
  return success();
}

} // namespace mlir::iara