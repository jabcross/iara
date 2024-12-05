//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/IaraOps.h"
#include "Iara/IaraDialect.h"
#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include "Util/rational.h"
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
  for (auto &type : tensor_types) {
    if (!isa<TensorType>(type))
      type = RankedTensorType::get({1}, type);
  }

  assert(llvm::all_of(tensor_types, [](Type t) { return isa<TensorType>(t); }));
  auto memref_types = tensor_types | Map([](auto type) {
                        auto tensor_type = cast<TensorType>(type);
                        return MemRefType::get(tensor_type.getShape(),
                                               tensor_type.getElementType());
                      }) |
                      Into<SmallVector<Type>>();
  return builder.getFunctionType(memref_types, TypeRange{});
}

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

bool ActorOp::hasInterface() {
  for (auto _ : getParams()) {
    return true;
  }
  for (Operation &op : getOps()) {
    if (isa<InPortOp>(&op) or isa<OutPortOp>(&op)) {
      return true;
    }
  }
  return false;
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

func::CallOp NodeOp::convertToCallOp() {
  auto call_op = CREATE(func::CallOp, OpBuilder{*this}, getLoc(), getImpl(), {},
                        getOperands());
  erase();
  return call_op;
}

bool NodeOp::signatureMatches(ActorOp actor) {
  auto actor_inputs = actor.getAllInputTypes();
  SmallVector<Value> node_inputs{getIn()};
  for (auto i : getInout())
    node_inputs.push_back(i);
  if (actor_inputs.size() != node_inputs.size()) {
    return false;
  }
  for (auto [a, b] : llvm::zip(actor_inputs, node_inputs)) {
    if (a != b.getType())
      return false;
  }

  auto actor_outputs = actor.getAllOutputTypes();
  SmallVector<Value> node_outputs{getOut()};

  if (actor_outputs.size() != node_outputs.size())
    return false;

  for (auto [a, b] : llvm::zip(actor_outputs, node_outputs)) {
    if (a != b.getType()) {
      return false;
    }
  }

  return true;
}

::mlir::LogicalResult
NodeOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
  // Todo: better verification
  return success();
}

util::Rational EdgeOp::getFlowRatio() {
  return util::Rational(getTypeSize(getIn().getType()),
                        getTypeSize(getOut().getType()));
}

} // namespace mlir::iara