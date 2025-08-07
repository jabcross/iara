#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <span>

#ifdef IARA_DEBUGPRINT
  #include "IaraRuntime/util/DebugPrint.h"
  #include <mutex>
extern std::mutex debug_mutex;
#endif

extern std::span<VirtualFIFO_Node> iara_runtime_nodes;
extern std::span<VirtualFIFO_Edge> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide

// std::unordered_map<void *, int> allocated_ptrs;

extern "C" void iara_runtime_alloc(i64 seq, VirtualFIFO_Chunk *chunk) {
  chunk->allocated = (i8 *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, VirtualFIFO_Chunk *chunk) {

#ifdef IARA_DEBUGPRINT
  // assert(allocated_ptrs.contains(chunk->allocated));
  // assert(allocated_ptrs[chunk->allocated] > 0);
  debugPrintThreadColor("freeing ptr %#016lx\n", (size_t)chunk->allocated);
#endif
  free(chunk->allocated);
}

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {

  // #pragma omp taskwait

  for (auto &node : iara_runtime_nodes) {
    if (node.needs_priming()) {
      for (i64 i = graph_iteration * node.static_info.total_iter_firings,
               e = i + node.static_info.total_iter_firings;
           i < e;
           i++) {
        auto node_ptr = &node;
#ifndef IARA_DISABLE_OMP
  #pragma omp task firstprivate(node_ptr, i)
#endif
        node_ptr->prime(i);
      }
    }
  }
}

extern "C" void iara_runtime_wait() {
#ifndef IARA_DISABLE_OMP
  #pragma omp taskwait
#endif
}

extern "C" void iara_runtime_exec(void (*exec)()) {

  auto &nodes = iara_runtime_nodes;
  auto &edges = iara_runtime_edges;

#ifndef IARA_DISABLE_OMP
  #pragma omp parallel if (false)
  {
  #pragma omp single
    {
      exec();
    }
  }
#else
  exec();
#endif
}

extern "C" void iara_runtime_init() {

#ifdef IARA_DEBUGPRINT
  for (auto &node : iara_runtime_nodes) {
    node.static_info.dump();
  }
  for (auto &edge : iara_runtime_edges) {
    edge.static_info.dump();
  }
#endif

  for (auto &node : iara_runtime_nodes) {
    node.init();
  }
  for (auto &node : iara_runtime_nodes) {
    if (node.static_info.isAlloc())
      node.fireAlloc(0);
  }
}
