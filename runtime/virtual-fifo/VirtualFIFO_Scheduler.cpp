#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

extern std::span<VirtualFIFO_Node> iara_runtime_nodes;
extern std::span<VirtualFIFO_Edge> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide

// std::unordered_map<void *, int> allocated_ptrs;

extern "C" void iara_runtime_alloc(i64 seq, Chunk *chunk) {
  chunk->allocated = (i8 *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, Chunk *chunk) {
  // assert(allocated_ptrs.contains(chunk->allocated));
  // assert(allocated_ptrs[chunk->allocated] > 0);
  // fprintf(stderr, "freeing ptr %#016lx\n", (size_t)chunk->allocated);
  // fflush(stderr);
  free(chunk->allocated);
}

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {
  for (auto &node : iara_runtime_nodes) {
    if (node.needs_priming()) {
      for (i64 i = graph_iteration * node.info.total_iter_firings,
               e = i + node.info.total_iter_firings;
           i < e;
           i++) {
#pragma omp task
        node.prime(i);
      }
    }
  }
}

extern "C" void iara_runtime_init(i64 num_threads) {
  iara_runtime_num_threads = num_threads;
  for (auto &node : iara_runtime_nodes) {
    node.init();
  }
  for (auto &node : iara_runtime_nodes) {
    if (node.info.isAlloc())
      node.fireAlloc(0);
  }
}
