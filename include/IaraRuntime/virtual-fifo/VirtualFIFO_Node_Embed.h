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

struct VirtualFIFO_Edge;

// Flags for VirtualFIFO_Node_CodegenInfo::flags
static constexpr uint8_t IARA_NODE_INPUTS_INLINE = 1 << 0;
static constexpr uint8_t IARA_NODE_NEEDS_PRIMING  = 1 << 1;

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
  union {
    struct {
      iara::int_edge start; // offset into iara_runtime_node_input_fifos_flat
      iara::int_edge count;
    } indirect;               // when !IARA_NODE_INPUTS_INLINE (input_count > 2)
    iara::int_edge inline_inputs[2]; // when IARA_NODE_INPUTS_INLINE (count <= 2)
  } input_fifos;
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

  inline bool needs_priming() const {
    return !runtime_info.isAlloc() && (runtime_info.flags & IARA_NODE_NEEDS_PRIMING);
  }

  void consume(i64 seq, VirtualFIFO_Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size, i64 first_buffer_size,
               i64 next_buffer_sizes, VirtualFIFO_Chunk chunk);
  void init();
  void prime(i64 seq);
  void fire(i64 seq, std::span<VirtualFIFO_Chunk>);
  void fireAlloc(i64 seq);
  void ensureAlloc(i64 firing);
  std::pair<i64, i64> getAllocDependentFirings(i64 iteration);
  void kickstart_alloc(i64 graph_iteration);
};

void iara_runtime_node_init(VirtualFIFO_Node *node);

} // extern "C"

#endif // IARA_RUNTIME_SDF_NODE_EMBED_H
