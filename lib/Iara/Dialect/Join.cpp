#include "Iara/Dialect/Join.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/OpCreateHelper.h"
#include <cassert>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinTypes.h>

namespace iara::dialect {

// Insert an N-ary join: N logic (`none`) inputs, one logic output. Firing
// threshold is N tokens (one per input) via the F1/W5 logic-gating mechanism —
// no data buffer, no alloc/dealloc. Downstream of the join goes a dealloc
// (all-read-only broadcast: free after all readers finish) or a gated consumer.
NodeOp insertJoin(llvm::ArrayRef<mlir::Value> logicInputs) {
  assert(!logicInputs.empty() && "join needs at least one logic input");

  // Insert after the latest-defined input so every operand dominates the join.
  mlir::Operation *insert_after = logicInputs.front().getDefiningOp();
  for (auto v : logicInputs) {
    auto *op = v.getDefiningOp();
    if (op && insert_after && insert_after->isBeforeInBlock(op))
      insert_after = op;
  }
  assert(insert_after && "logic inputs must be op results");

  auto builder = mlir::OpBuilder(insert_after);
  builder.setInsertionPointAfter(insert_after);

  auto none = mlir::NoneType::get(builder.getContext());
  auto join = CREATE(NodeOp,
                     builder,
                     insert_after->getLoc(),
                     mlir::TypeRange{none},
                     "iara_join",
                     mlir::ValueRange{},
                     /*in=*/logicInputs,
                     /*inout=*/mlir::ValueRange{});
  return join;
}

} // namespace iara::dialect
