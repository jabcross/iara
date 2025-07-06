#ifndef IARARUNTIME_RINGBUFFER_RINGBUFFERNODE_H
#define IARARUNTIME_RINGBUFFER_RINGBUFFERNODE_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include <cstddef>

struct RingBuffer_Edge;

struct RingBuffer_Node {

  struct StaticInfo {
    i64 id = -1;
    i64 input_bytes = -1;
    i64 num_ins = -1;
    i64 num_inouts = -1;
    i64 num_outs = -1;
    i64 working_memory_size;
    i64 rank = -1;
    i64 total_iter_firings = -1; // For normal nodes, it is the number of times
                                 // it fires in an iteration.
                                 // For alloc nodes, it is the number of firings
                                 // that depend on a (delay-less) block
  };

  using WrapperType = void(RingBuffer_Chunk *args);

  static constexpr size_t static_info_num_fields = 8;

  // data
  char *name;
  StaticInfo info;
  WrapperType *wrapper;
  Span<RingBuffer_Edge *> input_fifos;
  Span<RingBuffer_Edge *> output_fifos;

  // methods

  void init();

  void run_iteration();
};

#endif // IARA_RUNTIME_SDF_NODE_H
