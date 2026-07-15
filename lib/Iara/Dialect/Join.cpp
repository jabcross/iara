#include "Iara/Dialect/Join.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/OpCreateHelper.h"
#include <cassert>
#include <llvm/ADT/SmallVector.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinTypes.h>

namespace iara::dialect {

// Insert a join that owns a read-only-broadcast buffer. It takes the buffer as
// its single data (`in`) input plus N logic (`none`) inputs — one per read-only
// reader that borrows the buffer — and produces no output. GMMN threads the
// data input into a pass-through + dealloc as for any consumer, and W5 logic
// gating makes it fire (and thus free the buffer) only after every reader has
// signalled. The kernel is a no-op (`iara_join`, defined in the runtime).
NodeOp insertJoin(mlir::Value dataInput, llvm::ArrayRef<mlir::Value> logicInputs) {
  assert(dataInput && "join needs a data (buffer) input");

  llvm::SmallVector<mlir::Value> ins;
  ins.push_back(dataInput);
  ins.append(logicInputs.begin(), logicInputs.end());

  // Insert after the latest-defined input so every operand dominates the join.
  mlir::Operation *insert_after = dataInput.getDefiningOp();
  for (auto v : ins) {
    auto *op = v.getDefiningOp();
    if (op && insert_after && insert_after->isBeforeInBlock(op))
      insert_after = op;
  }
  assert(insert_after && "join inputs must be op results");

  auto builder = mlir::OpBuilder(insert_after);
  builder.setInsertionPointAfter(insert_after);

  auto join = CREATE(NodeOp,
                     builder,
                     insert_after->getLoc(),
                     /*results=*/mlir::TypeRange{},
                     "iara_join",
                     /*params=*/mlir::ValueRange{},
                     /*in=*/ins,
                     /*inout=*/mlir::ValueRange{});
  return join;
}

} // namespace iara::dialect
