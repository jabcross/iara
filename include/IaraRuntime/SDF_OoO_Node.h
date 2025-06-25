#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/Chunk.h"
#include <cstddef>

struct SDF_OoO_FIFO;

struct SDF_OoO_Node {

  struct NormalSemaphore;
  struct AllocSemaphore;

  union Semaphore {
    std::nullptr_t null;
    NormalSemaphore *normal;
    AllocSemaphore *alloc;
  };

  using WrapperType = void(i64 seq, Chunk *args);

  // WARNING: Be careful! These two have the same layout.
  struct StaticInfo {
    i64 id = -1;
    i64 input_bytes = -1;
    i64 num_inputs = -1;
    i64 rank = -1;
    i64 total_iter_firings = -1; // For normal nodes, it is the number of times
                                 // it fires in an iteration.
    i64 needs_priming = 1; // for normal nodes: 0 if can run whenever the inputs
                           // are ready, 1 if needs to be scheduled by run_iter

    bool isAlloc() { return input_bytes == -2; }
    bool isDealloc() { return input_bytes == -3; }
  };

  static constexpr size_t static_info_num_fields = 6;

  // END WARNING

  // data
  char *name;
  StaticInfo info;
  WrapperType *wrapper;
  Span<SDF_OoO_FIFO *> input_fifos;
  Span<SDF_OoO_FIFO *> output_fifos;
  Semaphore sema_variant{nullptr};

  // methods

  inline bool needs_priming() { return !info.isAlloc() && info.needs_priming; }

  void consume(i64 seq, Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size,
               i64 first_buffer_size,
               i64 next_buffer_sizes,
               Chunk chunk);
  void init();

  // Cause this firing to execute.
  void prime(i64 seq);

  void fire(i64 seq, Span<Chunk>);

  void fireAlloc(i64 seq);

  void ensureAlloc(i64 firing);

  std::pair<i64, i64> getAllocDependentFirings(i64 iteration);

  void kickstart_alloc(i64 graph_iteration);
};

extern "C" void iara_runtime_node_init(SDF_OoO_Node *node);

#endif // IARA_RUNTIME_SDF_NODE_H
