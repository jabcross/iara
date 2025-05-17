#include "Iara/IaraOps.h"
#include "Util/MlirUtil.h"
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <sstream>

namespace mlir {

namespace iara {

namespace mlir_util {

size_t getTypeTokenCount(Type type) {
  if (auto ranked_tensor_type =
          llvm::dyn_cast_if_present<RankedTensorType>(type)) {
    return ranked_tensor_type.getNumElements();
  }
  return 1;
}

// returns the size of the type in bytes
size_t getTypeSize(Value value) {
  auto dl = mlir::DataLayout::closest(value.getDefiningOp());
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

std::pair<func::FuncOp, OpBuilder>
createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                Location loc) {
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

StringRef getCTypeName(mlir::Type type) {
  auto ctx = type.getContext();
  if (type == IntegerType::get(ctx, 64))
    return "i64";
  if (type == IntegerType::get(ctx, 32))
    return "int32_t";
  llvm_unreachable("unimplemented");
}

func::FuncOp getOrGenFuncDecl(func::CallOp call, bool use_llvm_pointers) {
  auto module = call->getParentOfType<ModuleOp>();
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());

  if (auto func = module.lookupSymbol<func::FuncOp>(call.getCallee()))
    return func;

  auto rv = CREATE(
      func::FuncOp, builder, module.getLoc(), call.getCallee(),
      builder.getFunctionType(call->getOperandTypes(), call->getResultTypes()));
  rv->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
  rv.setVisibility(mlir::SymbolTable::Visibility::Private);
  return rv;
}

} // namespace mlir_util

} // namespace iara

} // namespace mlir