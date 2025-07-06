#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include <cstddef>

struct VirtualFIFO_Edge;

struct VirtualFIFO_Node {

  struct NormalSemaphore;
  struct AllocSemaphore;

  union Semaphore {
    std::nullptr_t null;
    NormalSemaphore *normal;
    AllocSemaphore *alloc;
  };

  using WrapperType = void(i64 seq, VirtualFIFO_Chunk *args);

  // WARNING: Be careful! These two have the same layout.
  struct StaticInfo {
    i64 id = -1;
    i64 arg_bytes = -1;
    i64 num_args = -1;
    i64 rank = -1;
    i64 total_iter_firings = -1; // For normal nodes, it is the number of times
                                 // it fires in an iteration.
                                 // For alloc nodes, it is the number of firings
                                 // that depend on a (delay-less) block
    i64 needs_priming = 1; // for normal nodes: 0 if can run whenever the inputs
                           // are ready, 1 if needs to be scheduled by run_iter

    bool isAlloc() { return arg_bytes == -2; }
    bool isDealloc() { return arg_bytes == -3; }

    inline void dump() {
      fprintf(stderr, "Dumping nodeinfo {\n");
      fprintf(stderr, "id = %ld\n", id);
      fprintf(stderr, "input_bytes = %ld\n", arg_bytes);
      fprintf(stderr, "num_args = %ld\n", num_args);
      fprintf(stderr, "rank = %ld\n", rank);
      fprintf(stderr, "total_iter_firings = %ld\n", total_iter_firings);
      fprintf(stderr, "needs_priming = %ld\n", needs_priming);
    }
  };

  static constexpr size_t static_info_num_fields = 6;

  // END WARNING

  // data
  char *name;
  StaticInfo info;
  WrapperType *wrapper;
  Span<VirtualFIFO_Edge *> input_fifos;
  Span<VirtualFIFO_Edge *> output_fifos;
  Semaphore sema_variant{nullptr};

  // methods

  inline bool needs_priming() { return !info.isAlloc() && info.needs_priming; }

  void
  consume(i64 seq, VirtualFIFO_Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size,
               i64 first_buffer_size,
               i64 next_buffer_sizes,
               VirtualFIFO_Chunk chunk);
  void init();

  // Cause this firing to execute.
  void prime(i64 seq);

  void fire(i64 seq, Span<VirtualFIFO_Chunk>);

  void fireAlloc(i64 seq);

  void ensureAlloc(i64 firing);

  std::pair<i64, i64> getAllocDependentFirings(i64 iteration);

  void kickstart_alloc(i64 graph_iteration);
};

extern "C" void iara_runtime_node_init(VirtualFIFO_Node *node);

#endif // IARA_RUNTIME_SDF_NODE_H
