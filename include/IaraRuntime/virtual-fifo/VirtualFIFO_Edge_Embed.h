#ifndef IARA_RUNTIME_SDF_OOO_FIFO_EMBED_H
#define IARA_RUNTIME_SDF_OOO_FIFO_EMBED_H

// Strategy B (embed sidecar) edge layout.
// Included by VirtualFIFO_Edge.h when IARA_VFIFO_DATA_STORAGE_EMBED is defined.

#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>

struct VirtualFIFO_Node;
struct VirtualFIFO_Edge;

extern "C" {

struct ConsData {
  i64 seq;
  i64 offset;
  i64 size;
};

// All runtime-relevant edge fields (renamed from VirtualFIFO_Edge_StaticInfo;
// field widths unchanged, that's a separate cleanup pass).
struct VirtualFIFO_Edge_RuntimeInfo {
  i64 local_index = -1;
  i64 prod_rate   = -1;
  i64 cons_rate   = -1;
  i64 cons_arg_idx = -1;
  i64 delay_offset = -1;
  i64 delay_size   = -1;
  i64 block_size_with_delays = -1;
  i64 block_size_no_delays   = -1;
  i64 prod_alpha = -1;
  i64 prod_beta  = -1;
  i64 cons_alpha = -1;
  i64 cons_beta  = -1;

  inline ConsData getConsumerSlice(i64 virtual_offset) {
    assert(cons_rate != -1);
    if (virtual_offset < block_size_with_delays) {
      auto [seq, cons_offset] = lldiv(virtual_offset - delay_offset, cons_rate);
      auto size = cons_rate - cons_offset;
      return {seq, cons_offset, size};
    }
    auto no_delays_offset = virtual_offset - block_size_with_delays;
    auto [seq, cons_offset] = lldiv(no_delays_offset, cons_rate);
    auto size = cons_rate - cons_offset;
    return {seq + cons_alpha, cons_offset, size};
  }

  inline void dump() {
    fprintf(stderr, "Dumping EdgeRuntimeInfo {\n");
    fprintf(stderr, "  local_index = %ld\n", local_index);
    fprintf(stderr, "  prod_rate = %ld\n", prod_rate);
    fprintf(stderr, "  cons_rate = %ld\n", cons_rate);
    fprintf(stderr, "  cons_arg_idx = %ld\n", cons_arg_idx);
    fprintf(stderr, "  delay_offset = %ld\n", delay_offset);
    fprintf(stderr, "  delay_size = %ld\n", delay_size);
    fprintf(stderr, "  block_size_with_delays = %ld\n", block_size_with_delays);
    fprintf(stderr, "  block_size_no_delays = %ld\n", block_size_no_delays);
    fprintf(stderr, "  prod_alpha = %ld\n", prod_alpha);
    fprintf(stderr, "  prod_beta = %ld\n", prod_beta);
    fprintf(stderr, "  cons_alpha = %ld\n", cons_alpha);
    fprintf(stderr, "  cons_beta = %ld }\n", cons_beta);
  }
};

// Codegen-determined immutable data. Integer indices only — no pointers, no .rela.
// Chain successor is implicit: e+1 when cons_rate >= 0 (end-of-chain: cons_rate < 0).
struct VirtualFIFO_Edge_CodegenInfo {
  iara::int_node consumer_idx;   // index into iara_runtime_nodes
  iara::int_node producer_idx;
  iara::int_node alloc_node_idx; // first node of inout chain
  uint32_t delay_start;          // byte offset into iara_runtime_edge_delays_flat
};

struct VirtualFIFO_Edge {
  VirtualFIFO_Edge_RuntimeInfo runtime_info;
  VirtualFIFO_Edge_CodegenInfo codegen_info;

  void push(VirtualFIFO_Chunk chunk);
  void propagate_delays(VirtualFIFO_Chunk chunk);

  inline i64 getRemainingDelay() {
    return runtime_info.delay_offset + runtime_info.delay_size;
  }

  inline std::pair<i64, i64> firingOfConsToVirtualOffsetRange(i64 cons_seq) {
    i64 zero  = runtime_info.block_size_with_delays - runtime_info.cons_rate * runtime_info.cons_alpha;
    i64 begin = zero + cons_seq * runtime_info.cons_rate;
    i64 end   = begin + runtime_info.cons_rate;
    return {begin, end};
  }

  inline i64 getSingleBlockNumberFromVirtualOffset(i64 virtual_offset) {
    if (virtual_offset < runtime_info.block_size_with_delays)
      return 0;
    return 1 + (virtual_offset - runtime_info.block_size_with_delays) /
                   runtime_info.block_size_no_delays;
  }

  inline static VirtualFIFO_Edge *getNextInChain(VirtualFIFO_Edge *e) {
    // Implicit chain: successor is e+1 when cons_rate >= 0 (end-of-chain: cons_rate < 0).
    if (e->runtime_info.cons_rate < 0)
      return nullptr;
    return e + 1;
  }

  inline static std::pair<i64, i64> getConsFiringsFromVirtualOffsetRange(
      VirtualFIFO_Edge_RuntimeInfo &info, i64 begin, i64 end) {
    assert(end - begin >= 1);
    auto firing_begin = info.getConsumerSlice(begin).seq;
    auto firing_end   = info.getConsumerSlice(end - 1).seq + 1;
    return {firing_begin, firing_end};
  }
};

} // extern "C"

#endif // IARA_RUNTIME_SDF_OOO_FIFO_EMBED_H
