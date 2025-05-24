#ifndef IARA_RUNTIME_SDF_OOO_FIFO_H
#define IARA_RUNTIME_SDF_OOO_FIFO_H

#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_Buffer.h"
#include "Util/types.h"
#include <boost/describe/class.hpp>
#include <cstdlib>

struct SDF_OoO_Node;

// SDF Out of Order Fifo.
struct alignas(64) SDF_OoO_FIFO {
  struct StaticInfo {
    i64 local_index;  // Position in the chain of inout
    i64 prod_rate;    //
    i64 cons_rate;    //
    i64 cons_arg_idx; // index of the connected port in the consumer
    std::span<byte> delay_data;
    i64 delay_offset; // start of delay in first chunk
    i64 first_chunk_size;
    i64 next_chunk_sizes;
  };

  StaticInfo info;
  SDF_OoO_Buffer *buffer;
  SDF_OoO_Node *producer;
  SDF_OoO_Node *consumer;

  // Called by the producer actor. Chooses which firings of the consumer will
  // receive it and schedules their execution.

  void push(Chunk chunk);

private:
  PairOf<i64> get_consumer_seq(i64 ooo_offset);
};

#endif
