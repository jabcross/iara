#ifndef IARA_DIALECT_TIED_OPERANDS_H
#define IARA_DIALECT_TIED_OPERANDS_H

// SCAFFOLD — prototypes only, not wired into any pass yet.
// See agent_workspace/Sprint-2026-07-10/ownership-aliasing-design.md for the
// full design: decouples ownership (in/inout segment: read-only vs
// read-write) from aliasing (which result forwards which operand's buffer).
// Modeled on IREE's TiedOpInterface / MHLO's output_operand_aliases —
// prevailing third-party pattern for general (including read-only) result to
// operand aliasing; chosen over DestinationStyleOpInterface, which cannot
// express a result forwarding a *read-only* operand's buffer.

#include "Iara/Dialect/IaraOps.h"
#include <llvm/ADT/ArrayRef.h>
#include <mlir/IR/Value.h>
#include <optional>

namespace iara::dialect {

// TODO(F2): back this with a real `tied_operands` DenseI64ArrayAttr on
// NodeOp (result index -> operand index it aliases, or -1 = fresh memory) and
// a thin custom OpInterface (TiedOpInterface) so passes — ours and
// collaborators' — can query aliasing generically instead of hardcoding the
// attribute name. Attribute = serializable storage, round-trips through any
// MLIR tool; interface = the queryable contract.

// Returns the operand this result aliases, or std::nullopt if the result
// owns fresh memory (tied to -1).
std::optional<mlir::OpOperand *> getTiedOperand(mlir::OpResult result);

// Returns the result tied to this operand, or null if the operand is not
// forwarded by any result (i.e. this is its last use — a dealloc site).
mlir::OpResult getTiedResult(mlir::OpOperand &operand);

// Sets the tie: `result` forwards `operand`'s buffer. Both must belong to the
// same NodeOp.
void setTiedOperand(mlir::OpResult result, mlir::OpOperand &operand);

} // namespace iara::dialect

#endif // IARA_DIALECT_TIED_OPERANDS_H
