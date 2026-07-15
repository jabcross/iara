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

// No-op kernel for the join that owns an all-read-only broadcast buffer. It has
// one inout data port (the buffer) and N logic inputs; the buffer passes through
// to its dealloc automatically (inout), and the logic gating (W5) makes the join
// fire — hence free — only after every reader has signalled. Nothing to compute.
extern "C" void iara_join(void *) {}

extern "C" void iara_runtime_alloc(i64 seq, VirtualFIFO_Chunk *chunk) {
#ifdef IARA_MOCK_ALLOC
  chunk->allocated = (i8 *)iara_mock_alloc(chunk->data_size);
#else
  chunk->allocated = (i8 *)malloc(chunk->data_size);
#endif
}

extern "C" void iara_runtime_dealloc(i64 seq, VirtualFIFO_Chunk *chunk) {

  if (chunk->allocated != IARA_EXTERNALLY_MANAGED_MEMORY) {

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("freeing ptr %#016lx\n", (size_t)chunk->allocated);
#endif

#ifdef IARA_MOCK_ALLOC
    iara_mock_free(chunk->allocated); // skip the shared mock region
#else
    free(chunk->allocated);
#endif
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
#ifdef IARA_DATA_TRIGGERED_ALLOC
  // No priming. Release this iteration's firing window, then kick the sources
  // (nodes with no true inputs); everything downstream is data-driven through
  // consume() -> fire(). fire() allocates each node's own output buffers and
  // refuses firings past the released window.
  iara_data_alloc_run_iter = graph_iteration + 1;
  for (auto &node : iara_runtime_nodes) {
    if (node.runtime_info.isAlloc() || node.runtime_info.isDealloc())
      continue;
    if (node.trueInputBytes() != 0)
      continue; // not a source; will be triggered by its inputs arriving
    auto *node_ptr = &node;
    i64 total = node.runtime_info.total_iter_firings;
    for (i64 seq = graph_iteration * total; seq < (graph_iteration + 1) * total;
         seq++) {
      auto *data = (VirtualFIFO_Chunk *)calloc(node.runtime_info.num_args,
                                               sizeof(VirtualFIFO_Chunk));
      node_ptr->fire(seq, {data, (size_t)node.runtime_info.num_args});
    }
  }
  if (wait_for_tasks)
    iara_task_wait();
  return;
#endif

  if (wait_for_tasks) {
#pragma omp taskgroup
    {
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
}

extern "C" void iara_runtime_wait() { iara_task_wait(); }

extern "C" void iara_runtime_exec(void (*exec)()) {
#pragma omp parallel
  {
#pragma omp single
    {
      exec();
    }
    iara_task_wait();
  }
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

  if (sizeof(VirtualFIFO_Node) != 32 || sizeof(VirtualFIFO_Edge) != 112) {
    fprintf(stderr, "FATAL: struct layout mismatch "
            "(Node=%zu expect 32, Edge=%zu expect 112)\n",
            sizeof(VirtualFIFO_Node), sizeof(VirtualFIFO_Edge));
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

#ifndef IARA_DATA_TRIGGERED_ALLOC
  // Default mode: eagerly allocate block 0 of every buffer. In the
  // data-triggered-alloc mode the producing node allocates its own output
  // buffers inline in fire(), so this eager pass is skipped — except feedback
  // (delay) buffers, whose initial tokens must be seeded before the cascade.
  for (auto &node : iara_runtime_nodes) {
    if (node.runtime_info.isAlloc())
      node.fireAlloc(0);
  }
#else
  for (auto &node : iara_runtime_nodes)
    node.seedFeedbackDelays();
#endif
}

extern "C" void iara_runtime_shutdown() {
  // Shutdown parallelism runtime (e.g., EnkiTS scheduler)
  iara_parallelism_shutdown();
}
