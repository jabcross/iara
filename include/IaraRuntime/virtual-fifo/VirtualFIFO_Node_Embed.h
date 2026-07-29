#ifndef IARA_RUNTIME_SDF_NODE_EMBED_H
#define IARA_RUNTIME_SDF_NODE_EMBED_H

// Strategy B (embed sidecar) node layout. Natural alignment throughout
// (no __attribute__((packed))). The blob is written with fwrite(sizeof(Node))
// so padding is consistent between codegen and runtime.

#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>

// Opt-in data-triggered allocation mode: a node fires on its true inputs and
// allocates its own output buffers in fire(), with no prime() priming token or
// ensureAlloc() chain. Enabled per-application with -DIARA_DATA_TRIGGERED_ALLOC.
// Default (legacy) is the prime()/ensureAlloc() path. Validated on all current
// benchmarks (cholesky, degridder, SIFT, SIFT-photo) under vf-omp and vf-enkits;
// opt-in (not a global default) because general-case correctness is not yet
// proven, not because of any known failure. Theoretical race risk to watch:
// concurrent-writer gathers sharing one g_block_cache block, and cross-iteration
// self-timed feedback beyond the per-iteration release window.

struct VirtualFIFO_Edge;

// Flags for VirtualFIFO_Node_CodegenInfo::flags
static constexpr uint8_t IARA_NODE_INPUTS_INLINE = 1 << 0;
static constexpr uint8_t IARA_NODE_NEEDS_PRIMING  = 1 << 1;
// All-read-only broadcast: fire() dispatches to fireBroadcast(), which aliases
// the one input buffer to every reader (zero copy). The reader edges are the
// node's logic_out range (logic_out_start/count are overloaded to hold them,
// since a borrow broadcast has no real logic outputs). The single inout-chain
// output (index 0) carries the owned buffer to the join.
static constexpr uint8_t IARA_NODE_IS_BROADCAST   = 1 << 2;

// Expected struct sizes (verified at startup).
static constexpr size_t IARA_NODE_STRUCT_SIZE = 32;
static constexpr size_t IARA_EDGE_STRUCT_SIZE = 112;

extern "C" {

struct VirtualFIFO_NormalSemaphore;
struct VirtualFIFO_AllocSemaphore;
union VirtualFIFO_Node_Semaphore {
  std::nullptr_t null;
  VirtualFIFO_NormalSemaphore *normal;
  VirtualFIFO_AllocSemaphore *alloc;
};

// Runtime-relevant node fields. Absorbs former VirtualFIFO_Node_StaticInfo
// (minus id/rank which are compiler-only). Semaphore is zero-initialized in
// the blob and patched by iara_runtime_node_init at startup.
struct VirtualFIFO_Node_RuntimeInfo {
  i64 arg_bytes;                          // encodes NodeType for alloc/dealloc
  VirtualFIFO_Node_Semaphore sema_variant; // zero in blob; patched at init
  uint32_t total_iter_firings;
  iara::int_edge num_args;
  uint8_t flags;                          // IARA_NODE_INPUTS_INLINE | IARA_NODE_NEEDS_PRIMING
  // Reserved pad byte (keeps the node at 32 bytes). Formerly logic_in_bytes: a
  // u8 sum of a join's logic-input tokens that overflowed past 255 at high
  // parallelism. Removed — inputDependencyBytes() now reconstructs that sum in
  // i64 by walking the logic input edges the embed appends to this node's
  // input-fifo slice (cons_arg_idx == -1). Single source of truth (edge
  // cons_rate), no overflow, no struct growth.
  uint8_t reserved0_;

  bool isAlloc()   const { return arg_bytes == static_cast<i64>(NodeType::Alloc); }
  bool isDealloc() const { return arg_bytes == static_cast<i64>(NodeType::Dealloc); }

  inline void dump() {
    fprintf(stderr, "Dumping NodeRuntimeInfo {\n");
    fprintf(stderr, "  arg_bytes = %ld\n", arg_bytes);
    fprintf(stderr, "  total_iter_firings = %u\n", total_iter_firings);
    fprintf(stderr, "  num_args = %u\n", num_args);
    fprintf(stderr, "  flags = %u\n", flags);
  }
};

// Codegen-determined immutable data. Integer indices only — no pointers,
// no .rela entries.
struct VirtualFIFO_Node_CodegenInfo {
  u8 kernel_id; // index into the codegen-emitted iara_dispatch_kernel switch
  // Logic (control-only) outputs are stored contiguously in the edge array,
  // outside every inout chain. The producer pushes a token on each in fire().
  // Packed into the pad byte after kernel_id so the node stays 32 bytes.
  u8 logic_out_count;
  union {
    struct {
      iara::int_edge start; // offset into iara_runtime_node_input_fifos_flat
      iara::int_edge count;
    } indirect;               // when !IARA_NODE_INPUTS_INLINE (input_count > 2)
    iara::int_edge inline_inputs[2]; // when IARA_NODE_INPUTS_INLINE (count <= 2)
  } input_fifos;
  iara::int_edge logic_out_start; // index of first logic-output edge
};

struct VirtualFIFO_Node {
  VirtualFIFO_Node_RuntimeInfo runtime_info;
  VirtualFIFO_Node_CodegenInfo codegen_info;

  // Alloc nodes: output edge stored inline (kernel takes 0 args).
  // Normal nodes: outputs follow next_in_chain from inputs.
  iara::int_edge getNumOutputs() const;
  iara::int_edge getOutputEdge(iara::int_edge idx) const;
  iara::int_edge getNumInputs() const;
  iara::int_edge getInputEdge(iara::int_edge idx) const;

  // Full input-fifo slice = data inputs (num_args, the kernel args) followed by
  // any logic inputs (a join's reader tokens, cons_arg_idx == -1). getNumInputs
  // returns only num_args (arg-fill + output chain); this returns the whole
  // slice for the firing-threshold loop. Inline nodes never carry logic inputs
  // (the embed forces indirect when any exist), so their total == num_args.
  iara::int_edge getNumInputEdges() const {
    return (runtime_info.flags & IARA_NODE_INPUTS_INLINE)
               ? runtime_info.num_args
               : codegen_info.input_fifos.indirect.count;
  }

  // Logic (control-only) outputs: contiguous edge indices, delivered as tokens
  // in fire() (not part of the inout-chain output enumeration).
  iara::int_edge getNumLogicOutputs() const { return codegen_info.logic_out_count; }
  iara::int_edge getLogicOutputEdge(iara::int_edge idx) const {
    return codegen_info.logic_out_start + idx;
  }

  inline bool needs_priming() const {
    return !runtime_info.isAlloc() && (runtime_info.flags & IARA_NODE_NEEDS_PRIMING);
  }

  // Per-firing byte total of this node's dependency inputs (edges whose producer
  // is not an alloc node). Used by the data-triggered-alloc scheduling mode,
  // where a node fires once its dependency inputs arrive and its own output
  // buffers (alloc-fed inputs) are allocated inline by fire().
  i64 inputDependencyBytes() const;

  void consume(i64 seq, VirtualFIFO_Chunk chunk, i64 arg_idx, i64 offset_partial);
  // Deliver `tokens` control tokens for firing `seq` from a logic edge: bumps
  // the firing counter without occupying a kernel-arg slot (prime()-style).
  void consumeLogic(i64 seq, i64 tokens);
  void dealloc(i64 current_buffer_size, i64 first_buffer_size,
               i64 next_buffer_sizes, VirtualFIFO_Chunk chunk);
  void init();
  void prime(i64 seq);
  void fire(i64 seq, std::span<VirtualFIFO_Chunk>);
  // All-read-only broadcast fire (IARA_NODE_IS_BROADCAST). Aliases the one input
  // buffer to every reader: pushes it read-write to the owned chain output
  // (index 0, → the join that frees it once) and as a borrow (allocated=null,
  // same data) to each reader edge in the logic_out range.
  void fireBroadcast(i64 seq, std::span<VirtualFIFO_Chunk>);
  void fireAlloc(i64 seq);
  void ensureAlloc(i64 firing);
  std::pair<i64, i64> getAllocDependentFirings(i64 iteration);
  void kickstart_alloc(i64 graph_iteration);

  // Data-triggered-alloc mode: if this is an alloc node feeding a buffer with
  // delays (a feedback edge), allocate its block 0 and propagate the initial
  // delay tokens now, so the feedback consumer has its seed input before the
  // data-driven cascade starts. Cached so fire() reuses the same block.
  void seedFeedbackDelays();

  // Data-triggered-alloc mode: seed the front delay region of each DELAYED
  // borrow output of a broadcast, so a delayed-borrow reader's firing 0 gets its
  // initial instance (delayBorrowEnabled zero-copy feedback). No-op unless this
  // is a broadcast with a delayed borrow output.
  void seedBorrowDelays();
};

void iara_runtime_node_init(VirtualFIFO_Node *node);

// Number of graph iterations released so far (data-triggered-alloc mode).
// run_iteration() bumps it; fire() refuses firings with
// seq >= iara_data_alloc_run_iter * total_iter_firings so self-timed feedback
// edges cannot spill into a later iteration's firings before it is released.
extern i64 iara_data_alloc_run_iter;

} // extern "C"

#endif // IARA_RUNTIME_SDF_NODE_EMBED_H
