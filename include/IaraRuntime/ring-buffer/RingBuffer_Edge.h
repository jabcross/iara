#ifndef IARARUNTIME_RINGBUFFER_RINGBUFFEREDGE_H
#define IARARUNTIME_RINGBUFFER_RINGBUFFEREDGE_H

#include "Iara/Util/Span.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <cstdlib>

struct RingBuffer_Node;

// SDF Out of Order Fifo.
struct RingBuffer_Edge {

  struct StaticInfo {
    i64 id = -1;
    i64 prod_rate = -1;         //
    i64 prod_rate_aligned = -1; //
    i64 cons_rate = -1;         //
    i64 cons_rate_aligned = -1; //
    i64 cons_arg_idx = -1;      // index of the connected port in the consumer
    i64 delay_size = -1;

    StaticInfo() = default;

    StaticInfo(i64 id,
               i64 prod_rate,
               i64 prod_rate_aligned,
               i64 cons_rate,
               i64 cons_rate_aligned,
               i64 cons_arg_idx,
               i64 delay_size)
        : id(id), prod_rate(prod_rate), prod_rate_aligned(prod_rate_aligned),
          cons_rate(cons_rate), cons_rate_aligned(cons_rate_aligned),
          cons_arg_idx(cons_arg_idx), delay_size(delay_size) {}
  };

  static constexpr size_t static_info_num_fields = 7;

  char *name;
  StaticInfo info;
  Span<const char> delay_data;
  RingBuffer_Node *consumer;
  RingBuffer_Node *producer;
  moodycamel::BlockingConcurrentQueue<char> *queue;

  RingBuffer_Edge() = default;
  RingBuffer_Edge(char *name,
                  StaticInfo info,
                  Span<const char> delay_data,
                  RingBuffer_Node *consumer,
                  RingBuffer_Node *producer,
                  moodycamel::BlockingConcurrentQueue<char> *queue)
      : name(name), info(info), delay_data(delay_data), consumer(consumer),
        producer(producer), queue(queue) {};

  // allocates ring buffer and inserts delays
  void init();

  void push(Chunk chunk);
  void pop(Chunk &chunk);
};

#endif
