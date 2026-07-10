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

// TODO(F3): insert a join node with N logic inputs (one per reader/writer
// being joined) and a single logic output, wired via SDF.h::isLogicEdge
// edges. Firing threshold = N (one token per input, no data). Downstream of
// the join: a dealloc (all-read-only case) or a gated consumer (mixed case).
NodeOp insertJoin(llvm::ArrayRef<mlir::Value> logicInputs);

} // namespace iara::dialect

#endif // IARA_DIALECT_JOIN_H
