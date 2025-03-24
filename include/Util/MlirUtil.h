#ifndef MLIR_IARA_MLIR_UTIL_H
#define MLIR_IARA_MLIR_UTIL_H
#include "OpCreateHelper.h"
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/Types.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>
#include <source_location>

namespace mlir::iara::mlir_util {

size_t getTypeTokenCount(Type type);
size_t getTypeSize(Value value);
size_t getTypeSize(Type type, DataLayout dl);

std::string stringifyType(Type type);

func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc);
OpOperand &appendOperand(Operation *op, Value val);
void moveBlockAfter(Block *to_move, Block *after_this);
void viewGraph(Operation *op);

// Follows a linear chain of uses until an operation of the given type is
// found. Returns null if a fork, dead end or cycle is found.
template <class T> T followChainUntilNext(Value val) {
  if (!val.hasOneUse()) {
    return nullptr;
  }
  llvm::DenseSet<Operation *> visited;
  Operation *op = *val.getUsers().begin();

  while (op != nullptr) {
    if (visited.contains(op)) {
      return nullptr;
    }
    if (auto rv = llvm::dyn_cast<T>(op)) {
      return rv;
    }
    auto users = llvm::to_vector(op->getUsers());
    if (users.size() != 1) {
      return nullptr;
    }
    visited.insert(op);
    op = users[0];
  }
  llvm_unreachable("Should return inside while loop");
  return nullptr;
}

// Follows a linear chain of uses backwards until an operation of the given
// type is found. Returns null if a join, dead end or cycle is found.
template <class T> T followChainUntilPrevious(Value val) {
  llvm::DenseSet<Operation *> visited;
  Operation *op = val.getDefiningOp();

  while (op != nullptr) {
    if (visited.contains(op)) {
      return nullptr;
    }
    if (auto rv = llvm::dyn_cast<T>(op)) {
      return rv;
    }
    if (op->getNumOperands() != 1) {
      return nullptr;
    }
    visited.insert(op);
    op = op->getOperand(0).getDefiningOp();
  }
  llvm_unreachable("Should return inside while loop");
  return nullptr;
}

// Generates an int constant for the given function and value
mlir::Value getIntConstant(func::FuncOp func, IntegerAttr val);

mlir::Value getIntConstant(func::FuncOp func, size_t value);

// generate C name for this type (for use on C header codegen)
StringRef getCTypeName(mlir::Type type);

} // namespace mlir::iara::mlir_util

#endif