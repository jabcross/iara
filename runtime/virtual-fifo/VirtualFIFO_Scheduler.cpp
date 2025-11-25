#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/common/IO.h"
#include "IaraRuntime/common/WorkStealingBackend.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <locale>
#include <span>
#include <vector>

#ifdef IARA_DEBUGPRINT
  #include "IaraRuntime/util/DebugPrint.h"
  #include <mutex>
extern std::mutex debug_mutex;
#endif

extern std::span<VirtualFIFO_Node> iara_runtime_nodes;
extern std::span<VirtualFIFO_Edge> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide

// This is always an invalid pointer in current 64 bit architectures (different
// 63rd and 62nd bits)
#define IARA_EXTERNALLY_MANAGED_MEMORY                                         \
  ((i8 *)(-1 ^ (1ul << (sizeof(i8 *) * 8 - 2))))

// std::unordered_map<void *, int> allocated_ptrs;

extern "C" void iara_runtime_alloc(i64 seq, VirtualFIFO_Chunk *chunk) {
  chunk->allocated = (i8 *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, VirtualFIFO_Chunk *chunk) {

  if (chunk->owns_memory) {

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("freeing ptr %#016lx\n", (size_t)chunk->allocated);
#endif

    free(chunk->allocated);
  }
#ifdef IARA_DEBUGPRINT
  else {
    debugPrintThreadColor("releasing externally managed memory at %#016lx\n",
                          (size_t)chunk->allocated);
  }
#endif
}

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {

  for (auto &node : iara_runtime_nodes) {
    if (node.needs_priming()) {
      for (i64 i = graph_iteration * node.static_info.total_iter_firings,
               e = i + node.static_info.total_iter_firings;
           i < e;
           i++) {
        auto node_ptr = &node;
        IARA_TASK_SUBMIT(node_ptr->prime(i), node_ptr, i);
      }
    }
  }
}

extern "C" void iara_runtime_wait() { IARA_TASK_WAIT(); }

extern "C" void iara_runtime_exec(void (*exec)()) {

  auto &nodes = iara_runtime_nodes;
  auto &edges = iara_runtime_edges;

  IARA_TASK_PARALLEL(IARA_TASK_SINGLE(exec();));
}

// Global storage for I/O sources and sinks
static std::vector<IaraSource *> iara_runtime_sources;
static std::vector<IaraSink *> iara_runtime_sinks;

extern "C" void iara_runtime_set_io(int num_io_ports, ...) {
  va_list args;
  va_start(args, num_io_ports);

  iara_runtime_sources.clear();
  iara_runtime_sinks.clear();

  // Collect all I/O ports from variadic arguments
  for (int i = 0; i < num_io_ports; i++) {
    void *port = va_arg(args, void *);
    // For now, we assume sources come first, then sinks
    // TODO: Add proper type discrimination or ordering metadata
    if (i < num_io_ports / 2) {
      iara_runtime_sources.push_back((IaraSource *)port);
    } else {
      iara_runtime_sinks.push_back((IaraSink *)port);
    }
  }

  va_end(args);
}

extern "C" void iara_runtime_init() {

  setlocale(LC_NUMERIC, "");

  // Initialize parallelism runtime (e.g., EnkiTS scheduler)
  iara_parallelism_init();

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

  // Provide input chunks from sources to scatter nodes
  // For now, assume first source goes to first scatter node
  // TODO: Add proper mapping from sources to nodes
  if (!iara_runtime_sources.empty()) {
    for (auto &node : iara_runtime_nodes) {
      if (node.static_info.isScatter()) {
        // Inject the source chunk into the scatter node
        // The scatter node will partition it and push to its output FIFOs
        auto &source = *iara_runtime_sources[0];
        node.consume(0, source.chunk, 0, 0);
        break;
      }
    }
  }

  for (auto &node : iara_runtime_nodes) {
    if (node.static_info.isAlloc())
      node.fireAlloc(0);
  }
}

extern "C" void iara_runtime_shutdown() {
  // Shutdown parallelism runtime (e.g., EnkiTS scheduler)
  iara_parallelism_shutdown();
}
