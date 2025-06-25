#ifndef IARA_RUNTIME_SDF_OOO_FIFO_H
#define IARA_RUNTIME_SDF_OOO_FIFO_H

#include "Iara/Util/Span.h"
#include "IaraRuntime/Chunk.h"
#include <cstdlib>

struct SDF_OoO_Node;

// SDF Out of Order Fifo.
struct SDF_OoO_FIFO {

  struct ConsData {
    i64 seq;
    i64 offset;
    i64 size;
  };

  struct StaticInfo {
    i64 id = -1;
    i64 local_index = -1;  // Position in the chain of inout
    i64 prod_rate = -1;    //
    i64 cons_rate = -1;    //
    i64 cons_arg_idx = -1; // index of the connected port in the consumer
    i64 delay_offset = -1; // start of delay in first chunk
    i64 delay_size = -1;

    // Result of analyzeVirtualBufferSizes
    i64 block_size_with_delays = -1;
    i64 block_size_no_delays = -1;
    i64 prod_alpha = -1;
    i64 prod_beta = -1;
    i64 cons_alpha = -1;
    i64 cons_beta = -1;

    // Given a global offset, return which firing of the consumer will consume
    // it, as well as the offset within that consumption, and its size.
    ConsData getConsumerSlice(i64 virtual_offset);
  };

  static constexpr size_t static_info_num_fields = 13;

  char *name;
  StaticInfo info;
  Span<const char> delay_data;
  SDF_OoO_Node *consumer;
  SDF_OoO_Node *producer;
  SDF_OoO_Node *alloc_node;
  SDF_OoO_FIFO *next_in_chain;

  // Called by the producer actor. Chooses which firings of the consumer will
  // receive it and schedules their execution.

  void push(Chunk chunk);
  void propagate_delays(Chunk chunk);

  inline i64 getRemainingDelay() { return info.delay_offset + info.delay_size; }

  // Given an edge and its consumer's sequence number, return the range it
  // affects in the virtual buffer
  inline std::pair<i64, i64> firingOfConsToVirtualOffsetRange(i64 cons_seq) {
    i64 zero = info.block_size_with_delays - info.cons_rate * info.cons_alpha;
    i64 begin = zero + cons_seq * info.cons_rate;
    i64 end = begin + info.cons_rate;
    return {begin, end};
  }

  // Given a virtual offset, return which block contains its memory
  inline i64 getSingleBlockNumberFromVirtualOffset(i64 virtual_offset) {
    if (virtual_offset < info.block_size_with_delays)
      return 0;
    return 1 + (virtual_offset - info.block_size_with_delays) /
                   info.block_size_no_delays;
  }

  // Given a virtual offset range, return which firings overlap with it
  inline static std::pair<i64, i64>
  getFiringsFromVirtualOffsetRange(StaticInfo &info, i64 begin, i64 end) {
    assert(end - begin >= 1);
    auto firing_begin = info.getConsumerSlice(begin).seq;
    auto firing_end = info.getConsumerSlice(end - 1).seq + 1;
    return {firing_begin, firing_end};
  }
};

#endif
