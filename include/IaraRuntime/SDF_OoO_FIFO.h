#ifndef IARA_RUNTIME_SDF_OOO_FIFO_H
#define IARA_RUNTIME_SDF_OOO_FIFO_H

#include "Iara/Util/Span.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include <boost/describe/class.hpp>
#include <cstdlib>

struct SDF_OoO_Node;

// SDF Out of Order Fifo.
struct alignas(64) SDF_OoO_FIFO {
  struct StaticInfo {
    i64 id = -1;
    i64 local_index = -1;  // Position in the chain of inout
    i64 prod_rate = -1;    //
    i64 cons_rate = -1;    //
    i64 cons_arg_idx = -1; // index of the connected port in the consumer
    i64 delay_offset = -1; // start of delay in first chunk
    i64 delay_size = -1;
    i64 first_chunk_size = -1;
    i64 next_chunk_sizes = -1;
    i64 prod_alpha = -1;
    i64 prod_beta = -1;
    i64 cons_alpha = -1;
    i64 cons_beta = -1;
  };

  StaticInfo info;
  Span<const char> delay_data;
  SDF_OoO_Node *consumer;
  SDF_OoO_Node *producer;

  // Called by the producer actor. Chooses which firings of the consumer will
  // receive it and schedules their execution.

  void push(Chunk chunk);

private:
  PairOf<i64> get_consumer_seq(i64 ooo_offset);
};

#endif
