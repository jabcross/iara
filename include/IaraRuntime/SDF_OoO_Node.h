#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/FirstLastSemaphore.h"
#include <span>

struct SDF_OoO_FIFO;

struct SDF_OoO_Node {
  struct StaticInfo {
    i64 id = -1;
    i64 input_bytes = -1;
    i64 num_inputs = 1;
    i64 rank = -1;
    i64 total_firings = -1;
  };
  using WrapperType = void(i64 seq, Chunk *args);

  StaticInfo info;
  void *wrapper;
  void *output_fifos;
  i64 num_output_fifos;

  struct EveryTimeArgs {
    Chunk data;
    i64 arg_idx;
    bool first_of_firing;
  };

  using ConsSeq = i64;

  struct None {};

  using SemaMap =
      first_last_semaphore::FirstLastSemaphoreMap<std::vector<Chunk>, None,
                                                  EveryTimeArgs, ConsSeq>;
  void *semaphore_map = new SemaMap{};

  void consume(i64 seq, Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size, i64 first_buffer_size,
               i64 next_buffer_sizes, Chunk chunk);

  void init();

private:
  void fire(i64 seq, std::vector<Chunk> &&);
};

extern "C" void iara_runtime_node_init(SDF_OoO_Node::WrapperType *wrapper);

#endif // IARA_RUNTIME_SDF_NODE_H
