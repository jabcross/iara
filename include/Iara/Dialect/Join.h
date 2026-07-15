#ifndef IARA_DIALECT_JOIN_H
#define IARA_DIALECT_JOIN_H

// SCAFFOLD — prototype only, not wired into any pass yet.
// See agent_workspace/Sprint-2026-07-10/broadcast-join-rewrite-design.md.
// An explicit join actor: gathers N logic (control-only) completions into a
// single downstream dependency (e.g. a dealloc, or a gated inout consumer's
// firing). Foundation for future multidimensional work: joining several
// overlapping *data* views of a buffer, with a Presburger solver deciding
// overlap/disjointness (paper section III, Fig. 6) — the N-ary join shape is
// shared between "N logic completions -> 1 free" (broadcast ownership, this
// sprint) and "N view writes -> 1 buffer, provably disjoint" (later).

#include "Iara/Dialect/IaraOps.h"
#include <llvm/ADT/ArrayRef.h>

namespace iara::dialect {

// Insert a join owning a read-only-broadcast buffer: `dataInput` is the buffer
// (a data value), `logicInputs` are one `none` token per read-only reader. The
// join has no output; GMMN generates its dealloc and W5 logic gating frees the
// buffer only after every reader has signalled. Kernel is a no-op (iara_join).
NodeOp insertJoin(mlir::Value dataInput, llvm::ArrayRef<mlir::Value> logicInputs);

} // namespace iara::dialect

#endif // IARA_DIALECT_JOIN_H
