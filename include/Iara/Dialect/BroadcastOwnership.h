#ifndef IARA_DIALECT_BROADCAST_OWNERSHIP_H
#define IARA_DIALECT_BROADCAST_OWNERSHIP_H

// SCAFFOLD — prototypes only, not wired into any pass yet.
// See agent_workspace/Sprint-2026-07-10/broadcast-join-rewrite-design.md for
// the full design. Root cause this targets: IaRa's broadcast physically
// memcpys each read-only fan-out output P times single-threaded (see
// lib/Iara/Dialect/Broadcast.cpp); this is the proven SIFT anti-scaling cause
// (MEMORY.md, sift-scaling-rootcause.md). Fix maps to LADF paper Fig. 2
// (pdfs/Multidimensional_dataflow_with_ownership_semantics-4.pdf): read-only
// borrowers share one buffer, freed after all readers finish via a join fed
// by logical (`none`-typed, see SDF.h::isLogicEdge) dependencies — not
// copies. Depends on F1 (logical dependencies, implemented) and F2
// (tied-operand ownership/aliasing split, scaffolded).

#include "Iara/Dialect/IaraOps.h"

namespace iara::dialect::broadcast {

enum class BroadcastOwnershipStrategy {
  // Current behavior (Broadcast.cpp default): copy every output but one,
  // regardless of downstream ownership. Always correct, never zero-copy.
  CopyAllButOne,
  // Paper Fig. 2(c): classify each output via F2's tied/relaxation rule.
  //   - all outputs read-only, shared (fan-out > 1): zero-copy — alias every
  //     output to the input buffer; free after all readers finish (N logic
  //     edges -> join -> single dealloc, see insertJoin below).
  //   - exactly one inout output: that consumer keeps the buffer; its firing
  //     is gated (via a logic edge) on the read-only readers finishing.
  //   - more than one inout output: the first (in appearance order) keeps
  //     the buffer, the rest get copies; the first's firing is gated on the
  //     copies and the read-only readers finishing.
  //   - outputs with differing rates: not representable by a single shared
  //     buffer — fall back to CopyAllButOne and emitWarning.
  FirstKeepsBuffer,
};

// TODO(F3): classify a broadcast's outputs (per-output read-only vs inout,
// via F2's tied-operand query + chain-fan-out count) and apply `strategy`,
// rewriting the broadcast node's copies/aliases and inserting the
// appropriate join (see Join.h) and logic edges. Falls back to
// CopyAllButOne + emitWarning on rate mismatch across outputs.
mlir::LogicalResult applyBroadcastOwnershipStrategy(
    NodeOp broadcast, BroadcastOwnershipStrategy strategy);

} // namespace iara::dialect::broadcast

#endif // IARA_DIALECT_BROADCAST_OWNERSHIP_H
