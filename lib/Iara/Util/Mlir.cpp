#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <sstream>

namespace iara::util::mlir {

size_t getTypeTokenCount(Type type) {
  if (auto ranked_tensor_type =
          llvm::dyn_cast_if_present<RankedTensorType>(type)) {
    return ranked_tensor_type.getNumElements();
  }
  return 1;
}

// returns the size of the type in bytes
size_t getTypeSize(Value value) {
  auto dl = DataLayout::closest(value.getDefiningOp());
  if (auto tensor = llvm::dyn_cast<TensorType>(value.getType())) {
    return tensor.getNumElements() * dl.getTypeSize(tensor.getElementType());
  }
  return dl.getTypeSize(value.getType());
}

// returns the size of the type in bytes
size_t getTypeSize(Type type, DataLayout dl) {
  if (auto tensor = llvm::dyn_cast<TensorType>(type)) {
    return tensor.getNumElements() * dl.getTypeSize(tensor.getElementType());
  }
  return dl.getTypeSize(type);
}

std::string stringifyType(Type type) {
  std::string vanilla_name;
  auto vanilla_name_s = llvm::raw_string_ostream(vanilla_name);
  type.print(vanilla_name_s);
  std::string safe_name;
  char c = vanilla_name[0];
  if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or c == '_') {
    safe_name += c;
  } else {
    safe_name += llvm::toHex(c);
  }

  for (auto c : StringRef(vanilla_name).drop_front(1)) {
    if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or
        (c >= '0' and c <= '9') or c == '_') {
      safe_name += c;
    } else {
      safe_name += llvm::toHex(c);
    }
  }
  return {safe_name};
}

std::pair<func::FuncOp, OpBuilder> createEmptyVoidFunctionWithBody(
    OpBuilder builder, StringRef name, Location loc) {
  auto rv =
      builder.create<func::FuncOp>(loc, name, builder.getFunctionType({}, {}));
  auto entry = rv.addEntryBlock();

  return {rv, builder.atBlockEnd(entry)};
}

OpOperand &appendOperand(Operation *op, Value val) {
  SmallVector<Value> operands = op->getOperands();
  operands.push_back(val);
  op->setOperands(operands);
  return op->getOpOperand(operands.size() - 1);
}

void moveBlockAfter(Block *to_move, Block *after_this) {
  if (after_this->getNextNode() != nullptr) {
    to_move->moveBefore(after_this->getNextNode());
    return;
  }
  // It's the last block of the region.
  auto region = after_this->getParent();
  auto &block = region->emplaceBlock();
  to_move->moveBefore(&block);
  block.erase();
}

void viewGraph(Operation *op) { op->getParentRegion()->viewGraph(); }

// Generates an int constant for the given function and value
Value getIntConstant(Block *block, IntegerAttr val) {
  auto builder = OpBuilder::atBlockBegin(block);
  auto rv =
      CREATE(arith::ConstantOp, builder, block->getParentOp()->getLoc(), val);
  return rv.getResult();
}

Value getIntConstant(Block *block, i64 value) {
  auto builder = OpBuilder(block->getParentOp()->getContext());
  return getIntConstant(
      block, builder.getIntegerAttr(builder.getIntegerType(64), value));
}

StringRef getCTypeName(Type type) {
  auto ctx = type.getContext();
  if (type == IntegerType::get(ctx, 64))
    return "i64";
  if (type == IntegerType::get(ctx, 32))
    return "int32_t";
  llvm_unreachable("unimplemented");
}

void ensureFuncDeclExists(func::CallOp call) {
  auto module = call->getParentOfType<ModuleOp>();
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());

  if (auto func = module.lookupSymbol(call.getCallee())) {
    if (auto mlirfunc = dyn_cast<func::FuncOp>(func)) {
    } else if (auto llvmfunc = dyn_cast<LLVM::LLVMFuncOp>(func)) {
    } else {
      func->dump();
      llvm_unreachable(
          "something is taking up this symbol, and it isn't a function.");
    }
    return;
  }

  auto decl = CREATE(
      func::FuncOp,
      builder,
      module.getLoc(),
      call.getCallee(),
      builder.getFunctionType(call->getOperandTypes(), call->getResultTypes()));
  decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
  decl.setVisibility(SymbolTable::Visibility::Private);
}

void ensureFuncDeclExists(LLVM::CallOp call) {
  auto module = call->getParentOfType<ModuleOp>();
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());

  auto callee_name = *call.getCallee();

  if (auto func = module.lookupSymbol(callee_name)) {
    if (auto mlirfunc = dyn_cast<func::FuncOp>(func)) {
    } else if (auto llvmfunc = dyn_cast<LLVM::LLVMFuncOp>(func)) {
    } else {
      func->dump();
      llvm_unreachable(
          "something is taking up this symbol, and it isn't a function.");
    }
    return;
  }

  auto decl = CREATE(LLVM::LLVMFuncOp,
                     builder,
                     module.getLoc(),
                     *call.getCallee(),
                     call.getCalleeFunctionType());
  decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
  decl.setVisibility(SymbolTable::Visibility::Private);
}

std::string tostring(Operation *op) {
  std::string rv;
  auto flags = OpPrintingFlags();
  flags.printGenericOpForm(true);
  flags.printValueUsers();
  flags.enableDebugInfo(true, true);
  llvm::raw_string_ostream s(rv);
  op->print(s, flags);
  // llvm::errs() << rv << "\n\n";
  return rv;
}

Operation *parent(Operation *op) { return op->getParentOp(); }

} // namespace iara::util::mlir
