#ifndef MLIR_IARA_MLIR_UTIL_H
#define MLIR_IARA_MLIR_UTIL_H
#include "Iara/Util/Types.h"
#include "OpCreateHelper.h"
#include <cstdint>
#include <functional>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>
#include <source_location>
#include <utility>

using namespace mlir;
namespace iara::util::mlir {

size_t getTypeTokenCount(Type type);
size_t getTypeSize(Value value);
size_t getTypeSize(Type type, DataLayout dl);

inline ModuleOp getModule(OpBuilder builder) {
  Operation *parent = builder.getInsertionBlock()->getParentOp();
  ModuleOp module = dyn_cast<ModuleOp>(parent);
  if (!module)
    module = parent->getParentOfType<ModuleOp>();
  return module;
}

std::string stringifyType(Type type);

inline i64 getNextPowerOf10(i64 n) {
  i64 rv = 1;
  while (n > 0) {
    rv *= 10;
    n /= 10;
  }
  return rv;
}

std::pair<func::FuncOp, OpBuilder>
createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
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
Value getIntConstant(Block *block, IntegerAttr val);

Value getIntConstant(Block *block, i64 value);

// generate C name for this type (for use on C header codegen)
StringRef getCTypeName(Type type);

template <class ValueType> struct MLIRTypeOf {};
template <> struct MLIRTypeOf<i64> {
  IntegerType get(MLIRContext *ctx) { return IntegerType::get(ctx, 64); }
};

class AttrAccessor {
public:
  Operation *op;
  StringRef attr_name;

  Attribute get() { return op->getAttr(attr_name); }

  void operator=(AttrAccessor) = delete;
  void operator=(AttrAccessor &) = delete;
  void operator=(AttrAccessor &&) = delete;

  IntegerAttr operator=(i64 value) {
    auto attr = OpBuilder{op}.getI64IntegerAttr(value);
    op->setAttr(attr_name, attr);
    return attr;
  }

  bool operator==(const char *string) {
    StringAttr attr = llvm::dyn_cast_if_present<StringAttr>(get());
    return attr && (attr.getValue() == string);
  };

  Attribute operator=(Attribute value) {
    op->setAttr(attr_name, value);
    return value;
  }

  operator bool() { return bool(op->getAttr(attr_name)); }

  explicit operator i64() {
    return op->getAttrOfType<IntegerAttr>(attr_name).getInt();
  };

  explicit operator StringRef() {
    return op->getAttrOfType<StringAttr>(attr_name).getValue();
  }

  operator Attribute() {
    if (op->hasAttr(attr_name))
      return op->getAttr(attr_name);
    return Attribute();
  }

  Attribute operator*() { return get(); }
};

struct AssertNonNull {};

template <class T> inline auto operator|(T &&lhs, AssertNonNull rhs) {
  if (!lhs) {
    llvm_unreachable("Something is null that shouldn't.");
  }
  return std::forward<T>(lhs);
}

func::FuncOp getOrGenFuncDecl(func::CallOp call);
LLVM::LLVMFuncOp getOrGenFuncDecl(LLVM::CallOp call);

} // namespace iara::util::mlir

#endif
