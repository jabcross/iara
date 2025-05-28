//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/rational.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Casting.h"
#include <assert.h>
#include <functional>
#include <iterator>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/Bufferization/IR/BufferizableOpInterface.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LLVM.h>

#define GET_OP_CLASSES
#include "Iara/Dialect/IaraOps.cpp.inc"

using namespace iara::util::range;
using namespace iara::util::mlir;

namespace iara {

bool ActorOp::isEmpty() {
  for (Operation &op : this->getOps()) {
    if (llvm::isa<NodeOp, EdgeOp>(&op))
      return false;
  }
  return true;
}

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
bool ActorOp::isKernelDeclaration() {
  if ((*this)->hasAttr("kernel-decl"))
    return true;
  if ((*this)->hasAttr("kernel-def"))
    return false;
  auto nodes = (*this).getOps<NodeOp>() | IntoVector();
  if (nodes.empty()) {
    (*this)->setAttr("kernel-decl", OpBuilder(*this).getUnitAttr());
    return true;
  }
  (*this)->setAttr("kernel-def", OpBuilder(*this).getUnitAttr());
  return false;
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
  assert(operand.getOwner() == getOperation());
  for (auto opd : this->getInout()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

bool NodeOp::isInoutOutput(OpResult &result) {
  assert(result.getOwner() == getOperation());
  if (result.getResultNumber() < getInout().size()) {
    return false;
  }
  return true;
}

bool NodeOp::isPureInInput(OpOperand &operand) {
  for (auto opd : this->getIn()) {
    if (opd == operand.get())
      return true;
  }
  return false;
}

llvm::SmallVector<Value> NodeOp::getAllInputs() {
  SmallVector<Value> rv;
  for (auto i : getIn()) {
    rv.push_back(i);
  }
  for (auto i : getInout()) {
    rv.push_back(i);
  }
  return rv;
}

llvm::SmallVector<mlir::Value> NodeOp::getAllOutputs() {
  return getOut() | Into<llvm::SmallVector<Value>>();
}

// Returns the operand that corresponds to this output, if they form an inout
// pair. Null otherwise.
Value NodeOp::getMatchingInoutInput(Value output) {
  for (auto [in, out] : getInoutPairs()) {
    if (out == output)
      return in;
  }
  return nullptr;
}

// Returns the result that corresponds to this input, if they form an inout
// pair. Null otherwise.
Value NodeOp::getMatchingInoutOutput(Value input) {
  for (auto [in, out] : getInoutPairs()) {
    if (in == input)
      return out;
  }
  return nullptr;
}

func::CallOp NodeOp::convertToCallOp() {
  auto call_op = CREATE(func::CallOp, OpBuilder{*this}, getLoc(), getImpl(), {},
                        getOperands());
  erase();
  return call_op;
}

FunctionType NodeOp::getKernelFunctionType() {
  auto builder = OpBuilder(*this);
  SmallVector<Type> types;

  for (auto p : getParams()) {
    types.push_back(p.getType());
  }
  for (auto v : getIn()) {
    types.push_back(v.getType());
  }
  for (auto v : getInout()) {
    types.push_back(v.getType());
  }
  for (auto v : getPureOuts()) {
    types.push_back(v.getType());
  }

  auto memref_types =
      llvm::to_vector(types | Map([](Type type) {
                        auto tensor = cast<RankedTensorType>(type);
                        return cast<Type>(MemRefType::get(
                            tensor.getShape(), tensor.getElementType()));
                      }));

  return builder.getFunctionType(memref_types, {});
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

bool NodeOp::isAlloc() { return getImpl() == "iara_runtime_alloc"; }

bool NodeOp::isDealloc() { return getImpl() == "iara_runtime_dealloc"; }

::mlir::LogicalResult
NodeOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
  // Todo: better verification
  return success();
}

// Input rate in bytes
i64 EdgeOp::getProdRate() { return getTypeSize(getIn()); }

// Output rate in bytes
i64 EdgeOp::getConsRate() { return getTypeSize(getOut()); }

util::Rational EdgeOp::getFlowRatio() {
  return util::Rational(getTypeSize(getIn()), getTypeSize(getOut()));
}

NodeOp EdgeOp::getProducerNode() {
  return llvm::dyn_cast_if_present<NodeOp>(getIn().getDefiningOp());
}
NodeOp EdgeOp::getConsumerNode() {
  auto users = llvm::to_vector(getOut().getUsers());
  if (users.size() != 1)
    llvm_unreachable("Should have a consumer");
  return llvm::dyn_cast<NodeOp>(users[0]);
}

// Follow an inout edge backwards, or return null.
EdgeOp followInoutChainBackwards(EdgeOp edge) {
  auto prev_node = edge.getProducerNode();
  if (auto in_port = prev_node.getMatchingInoutInput(edge.getIn())) {
    return followChainUntilPrevious<EdgeOp>(in_port);
  }
  return nullptr;
}

// Follow an inout edge forwards, or return null.
EdgeOp followInoutChainForwards(EdgeOp edge) {

  auto next_node = edge.getConsumerNode();
  if (auto out_port = next_node.getMatchingInoutOutput(edge.getOut())) {
    return followChainUntilNext<EdgeOp>(out_port);
  }
  return nullptr;
}

EdgeOp findFirstEdgeOfChain(EdgeOp edge) {
  auto prev = edge;
  while (prev) {
    edge = prev;
    prev = followInoutChainBackwards(prev);
  }
  return edge;
}

EdgeOp findLastEdgeOfChain(EdgeOp edge) {
  auto next = edge;
  while (next) {
    edge = next;
    next = followInoutChainForwards(next);
  }
  return edge;
}

} // namespace iara
