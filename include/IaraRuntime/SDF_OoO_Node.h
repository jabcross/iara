#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/FirstLastSemaphore.h"
#include "Util/types.h"
#include <span>

struct SDF_OoO_FIFO;

struct SDF_OoO_Node {
  struct StaticInfo {
    i64 input_bytes;
    i64 num_inputs;
  };
  StaticInfo info;
  void (*wrapper)(i64 seq, std::span<Chunk> args);
  i64 input_bytes;
  i64 num_inputs;
  std::span<SDF_OoO_FIFO> output_fifos;

  struct EveryTimeArgs {
    Chunk data;
    i64 arg_idx;
    bool first_of_firing;
  };

  using ConsSeq = i64;

  struct None {};

  first_last_semaphore::FirstLastSemaphoreMap<std::vector<Chunk>, None,
                                              EveryTimeArgs, ConsSeq>
      semaphore_map{};
  ParallelHashMap<i64, std::vector<Chunk>> arg_map;

  void consume(i64 seq, Chunk chunk, i64 arg_idx, i64 offset_partial);

  void init();

private:
  void fire(i64 seq, std::vector<Chunk> &&);
};

#endif // IARA_RUNTIME_SDF_NODE_H
