#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "Iara/Util/Span.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/FirstLastSemaphore.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include <cstddef>
#include <span>

struct SDF_OoO_FIFO;

struct SDF_OoO_Node {
  using WrapperType = void(i64 seq, Chunk *args);
  using ConsSeq = i64;
  struct None {};
  struct EveryTimeArgs {
    Chunk data;
    i64 arg_idx;
    bool first_of_firing;
  };
  using SemaMap =
      first_last_semaphore::FirstLastSemaphoreMap<std::vector<Chunk>, None,
                                                  EveryTimeArgs, ConsSeq>;

  struct StaticInfo {
    i64 id = -1;
    i64 input_bytes = 0;
    i64 num_inputs = 0;
    i64 rank = -1;
    i64 total_firings = -1;
  };

  // data
  StaticInfo info;
  WrapperType *wrapper;
  Span<SDF_OoO_FIFO *> output_fifos;
  void *semaphore_map = nullptr;

  void consume(i64 seq, Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size, i64 first_buffer_size,
               i64 next_buffer_sizes, Chunk chunk);
  void init();

private:
  void fire(i64 seq, std::vector<Chunk> &&);
};

extern "C" void iara_runtime_node_init(SDF_OoO_Node *node);

#endif // IARA_RUNTIME_SDF_NODE_H
