#ifndef IARA_RUNTIME_SDF_OOO_FIFO_H
#define IARA_RUNTIME_SDF_OOO_FIFO_H

#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include <cstdio>
#include <cstdlib>
#include <span>
#include <tuple>
#include <utility>

// #ifdef IARA_COMPILER
//   #include "boost/describe.hpp"
// #endif

extern "C"

{
struct VirtualFIFO_Node;
struct VirtualFIFO_Edge;

struct ConsData {
  i64 seq;
  i64 offset;
  i64 size;
};

struct VirtualFIFO_Edge_StaticInfo {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Edge_StaticInfo";
  using TupleType = std::
      tuple<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64>;

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

  inline void dump() {
    fprintf(stderr, "Dumping edgeinfo {\n");
    fprintf(stderr, "id = %ld\n", id);
    fprintf(stderr, "local_index = %ld\n", local_index);
    fprintf(stderr, "prod_rate = %ld\n", prod_rate);
    fprintf(stderr, "cons_rate = %ld\n", cons_rate);
    fprintf(stderr, "cons_arg_idx = %ld\n", cons_arg_idx);
    fprintf(stderr, "delay_offset = %ld\n", delay_offset);
    fprintf(stderr, "delay_size = %ld\n", delay_size);

    fprintf(stderr, "block_size_with_delays = %ld\n", block_size_with_delays);
    fprintf(stderr, "block_size_no_delays = %ld\n", block_size_no_delays);
    fprintf(stderr, "prod_alpha = %ld\n", prod_alpha);
    fprintf(stderr, "prod_beta = %ld\n", prod_beta);
    fprintf(stderr, "cons_alpha = %ld\n", cons_alpha);
    fprintf(stderr, "cons_beta = %ld }\n", cons_beta);
  }

  // #ifdef IARA_COMPILER
  //   BOOST_DESCRIBE_CLASS(StaticInfo,
  //                        (),
  //                        (id,
  //                         local_index,
  //                         prod_rate,
  //                         cons_rate,
  //                         cons_arg_idx,
  //                         delay_offset,
  //                         delay_size,
  //                         block_size_with_delays,
  //                         block_size_no_delays,
  //                         prod_alpha,
  //                         prod_beta,
  //                         cons_alpha,
  //                         cons_beta),
  //                        (),
  //                        ())
  // #endif
};

struct VirtualFIFO_Edge_CodegenInfo {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Edge_CodegenInfo";

  using TupleType = std::tuple<char *,
                               std::span<const char>,
                               VirtualFIFO_Node *,
                               VirtualFIFO_Node *,
                               VirtualFIFO_Node *,
                               VirtualFIFO_Edge *>;
  char *name;
  std::span<const char> delay_data;
  VirtualFIFO_Node *consumer;
  VirtualFIFO_Node *producer;
  VirtualFIFO_Node *alloc_node;
  VirtualFIFO_Edge *next_in_chain;
};

// SDF Out of Order Fifo.
struct VirtualFIFO_Edge {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Edge";

  using TupleType =
      std::tuple<VirtualFIFO_Edge_StaticInfo, VirtualFIFO_Edge_CodegenInfo>;

  VirtualFIFO_Edge_StaticInfo static_info;
  VirtualFIFO_Edge_CodegenInfo codegen_info;

  // Called by the producer actor. Chooses which firings of the consumer will
  // receive it and schedules their execution.

  void push(VirtualFIFO_Chunk chunk);
  void propagate_delays(VirtualFIFO_Chunk chunk);

  inline i64 getRemainingDelay() {
    return static_info.delay_offset + static_info.delay_size;
  }

  // Given an edge and its consumer's sequence number, return the range it
  // affects in the virtual buffer
  inline std::pair<i64, i64> firingOfConsToVirtualOffsetRange(i64 cons_seq) {
    // fprintf(stderr, "calculating offset from consumer firing %ld\n",
    // cons_seq); fprintf(
    //     stderr, "block size with delays is %ld\n",
    //     info.block_size_with_delays);
    // fprintf(stderr, "cons rate is %ld\n", info.cons_rate);
    // fprintf(stderr, "cons alpha is %ld\n", info.cons_alpha);
    // fflush(stderr);
    i64 zero = static_info.block_size_with_delays -
               static_info.cons_rate * static_info.cons_alpha;
    i64 begin = zero + cons_seq * static_info.cons_rate;
    i64 end = begin + static_info.cons_rate;
    return {begin, end};
  }

  // Given a virtual offset, return which block contains its memory
  inline i64 getSingleBlockNumberFromVirtualOffset(i64 virtual_offset) {
    // fprintf(
    //     stderr, "calculating block number from offset %ld\n",
    //     virtual_offset);
    // fprintf(
    //     stderr, "block size with delays is %ld\n",
    //     info.block_size_with_delays);
    // fprintf(stderr,
    //         "block size with no delays is %ld\n",
    //         info.block_size_no_delays);

    // fflush(stderr);

    if (virtual_offset < static_info.block_size_with_delays)
      return 0;
    return 1 + (virtual_offset - static_info.block_size_with_delays) /
                   static_info.block_size_no_delays;
  }

  // Given a virtual offset range, return which firings overlap with it
  inline static std::pair<i64, i64> getConsFiringsFromVirtualOffsetRange(
      VirtualFIFO_Edge_StaticInfo &info, i64 begin, i64 end) {
    assert(end - begin >= 1);
    auto firing_begin = info.getConsumerSlice(begin).seq;
    auto firing_end = info.getConsumerSlice(end - 1).seq + 1;
    return {firing_begin, firing_end};
  }
};
}

#endif
