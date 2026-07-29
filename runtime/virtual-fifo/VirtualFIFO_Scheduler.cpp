#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/common/IO.h"
#include "IaraRuntime/common/WorkStealingBackend.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include "IaraRuntime/virtual-fifo/MockAllocator.h"
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

// No-op kernel for a join node that gates buffer deallocation on all reader
// signals. Chained joins are possible: a read-only borrow may hierarchically
// sub-borrow views through another Broadcast, whose join then chains to the
// original join rather than directly to a dealloc. The join fires — hence frees
// the buffer — only after every reader has signalled. Nothing to compute.
extern "C" void iara_join(void *) {}

extern "C" void iara_runtime_alloc(i64 seq, VirtualFIFO_Chunk *chunk) {
  chunk->allocated = (i8 *)iara_malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, VirtualFIFO_Chunk *chunk) {

  if (chunk->allocated != IARA_EXTERNALLY_MANAGED_MEMORY) {

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("freeing ptr %#016lx\n", (size_t)chunk->allocated);
#endif

    iara_runtime_free(chunk->allocated);
  }
#ifdef IARA_DEBUGPRINT
  else {
    debugPrintThreadColor("releasing externally managed memory at %#016lx\n",
                          (size_t)chunk->allocated);
  }
#endif
}

// Released-iteration counter for the data-triggered-alloc mode (see header).
extern "C" {
i64 iara_data_alloc_run_iter = 0;
}

extern "C" void iara_runtime_run_iteration(i64 graph_iteration,
                                           int wait_for_tasks) {
#ifdef IARA_PRIMING_ALLOC
  if (wait_for_tasks) {
IARA_TASKGROUP_BEGIN
      for (auto &node : iara_runtime_nodes) {
        if (node.needs_priming()) {
          for (i64 i = graph_iteration * node.runtime_info.total_iter_firings,
                   e = i + node.runtime_info.total_iter_firings;
               i < e;
               i++) {
            auto node_ptr = &node;
            iara_submit_task([=]() { node_ptr->prime(i); });
          }
        }
      }
IARA_TASKGROUP_END
  } else {
    for (auto &node : iara_runtime_nodes) {
      if (node.needs_priming()) {
        for (i64 i = graph_iteration * node.runtime_info.total_iter_firings,
                 e = i + node.runtime_info.total_iter_firings;
             i < e;
             i++) {
          auto node_ptr = &node;
          iara_submit_task([=]() { node_ptr->prime(i); });
        }
      }
    }
  }
  return;
#endif

  // Data-triggered default: no priming. Release this iteration's firing
  // window, then kick the sources (nodes with no dependency inputs); everything
  // downstream is data-driven through consume() -> fire().
  iara_data_alloc_run_iter = graph_iteration + 1;
  auto kick_sources = [&]() {
    for (auto &node : iara_runtime_nodes) {
      if (node.runtime_info.isAlloc() || node.runtime_info.isDealloc())
        continue;
      if (node.inputDependencyBytes() != 0)
        continue; // not a source; will be triggered by its inputs arriving
      auto *node_ptr = &node;
      i64 total = node.runtime_info.total_iter_firings;
      for (i64 seq = graph_iteration * total;
           seq < (graph_iteration + 1) * total; seq++) {
        auto *data = (VirtualFIFO_Chunk *)calloc(node.runtime_info.num_args,
                                                 sizeof(VirtualFIFO_Chunk));
        node_ptr->fire(seq, {data, (size_t)node.runtime_info.num_args});
      }
    }
  };
  if (wait_for_tasks) {
IARA_TASKGROUP_BEGIN
    kick_sources();
IARA_TASKGROUP_END
    iara_task_wait();
  } else {
    kick_sources();
  }
}

extern "C" void iara_runtime_wait() { iara_task_wait(); }

extern "C" void iara_runtime_exec(void (*exec)()) {
  iara_parallel_exec([&]() {
    iara_single_exec([&]() { exec(); });
    iara_task_wait();
  });
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

extern "C" void iara_runtime_startup() {

  setlocale(LC_NUMERIC, "");

  if (sizeof(VirtualFIFO_Node) != IARA_NODE_STRUCT_SIZE || sizeof(VirtualFIFO_Edge) != IARA_EDGE_STRUCT_SIZE) {
    fprintf(stderr, "FATAL: struct layout mismatch "
            "(Node=%zu expect %zu, Edge=%zu expect %zu)\n",
            sizeof(VirtualFIFO_Node), (size_t)IARA_NODE_STRUCT_SIZE,
            sizeof(VirtualFIFO_Edge), (size_t)IARA_EDGE_STRUCT_SIZE);
    abort();
  }

  // Initialize parallelism runtime (e.g., EnkiTS scheduler)
  iara_parallelism_init();

#ifdef IARA_DEBUGPRINT
  for (auto &node : iara_runtime_nodes) {
    node.runtime_info.dump();
  }
  for (auto &edge : iara_runtime_edges) {
    edge.runtime_info.dump();
  }
#endif

  for (auto &node : iara_runtime_nodes) {
    node.init();
  }

  // Scatter/gather functionality removed - TODO: re-implement if needed
  // if (!iara_runtime_sources.empty()) {
  //   for (auto &node : iara_runtime_nodes) {
  //     // Handle source injection
  //   }
  // }

#ifdef IARA_PRIMING_ALLOC
  // Priming mode: eagerly allocate block 0 of every buffer.
  for (auto &node : iara_runtime_nodes) {
    if (node.runtime_info.isAlloc())
      node.fireAlloc(0);
  }
#else
  // Data-triggered default: seed feedback and delayed-borrow initial tokens
  // before the cascade. Output buffers are allocated inline in fire().
  for (auto &node : iara_runtime_nodes)
    node.seedFeedbackDelays();
  for (auto &node : iara_runtime_nodes)
    node.seedBorrowDelays();
#endif
}

extern "C" void iara_runtime_shutdown() {
  // Shutdown parallelism runtime (e.g., EnkiTS scheduler)
  iara_parallelism_shutdown();
}
