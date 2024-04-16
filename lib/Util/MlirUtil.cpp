#include "Util/MlirUtil.h"
#include <mlir/IR/Builders.h>

namespace mlir {
func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc) {
  auto rv =
      builder.create<func::FuncOp>(loc, name, builder.getFunctionType({}, {}));
  builder.atBlockEnd(rv.addEntryBlock()).create<func::ReturnOp>(loc);
  return rv;
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
  auto builder = OpBuilder{after_this->getParentOp()};
  auto region = after_this->getParent();
  auto &block = region->emplaceBlock();
  to_move->moveBefore(&block);
  block.erase();
}

} // namespace mlir
