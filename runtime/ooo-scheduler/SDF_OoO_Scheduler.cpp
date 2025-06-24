#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include <cassert>
#include <cstdlib>

extern std::span<SDF_OoO_Node> iara_runtime_nodes;
extern std::span<SDF_OoO_FIFO> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide

extern "C" void iara_runtime_alloc(i64 seq, Chunk *chunk) {
  chunk->allocated = (i8 *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, Chunk *chunk) {
  free(chunk->allocated);
}

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {
  for (auto &node : iara_runtime_nodes) {
    if (node.needs_priming()) {
      for (i64 i = graph_iteration * node.info.total_iter_firings,
               e = i + node.info.total_iter_firings;
           i < e;
           i++) {
        node.prime(i);
      }
    }
  }
}

extern "C" void iara_runtime_init(i64 num_threads) {
  for (auto &node : iara_runtime_nodes) {
    node.init();
  }
  for (auto &node : iara_runtime_nodes) {
    if (node.info.isAlloc())
      node.fireAlloc(0);
  }
}