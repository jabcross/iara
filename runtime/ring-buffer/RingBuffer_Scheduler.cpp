#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include "IaraRuntime/util/DebugPrint.h"
#include <cassert>
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <llvm/Support/ErrorHandling.h>
#include <queue>
#include <semaphore>
#include <thread>
#include <utility>
#include <vector>

extern Span<RingBuffer_Node> iara_runtime_nodes;
extern Span<RingBuffer_Edge> iara_runtime_edges;

#ifdef IARA_DEBUGPRINT
extern std::mutex debug_mutex;
#endif

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide
struct sema_wrapper {
  std::binary_semaphore sema;
  constexpr sema_wrapper() : sema(0) {};
  constexpr explicit sema_wrapper(auto count) : sema(count) {};
};

std::counting_semaphore<> iter_ended{0};

sema_wrapper *semas;

size_t getIndex(RingBuffer_Node &node) {
  return &node - &iara_runtime_nodes.asSpan().front();
}

struct node_thread {};

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {

#pragma omp taskgroup
  {
    for (auto &node : iara_runtime_nodes.asSpan()) {
      auto node_ptr = &node;
#pragma omp task firstprivate(node_ptr)
      {

#ifdef IARA_DEBUGPRINT
        debug_mutex.lock();
        debugPrintThreadColor("running iteration for node %ld\n",
                              node_ptr->info.id);
        debug_mutex.unlock();
#endif
        node_ptr->run_iteration();
      }
    }
  }
}

extern "C" void iara_runtime_exec(void (*exec)()) {
#pragma omp parallel
  {
#pragma omp single
    {
      exec();
    }
  }
}

extern "C" void iara_runtime_init() {

  auto threadnum = std::thread::hardware_concurrency();

  semas = new sema_wrapper[iara_runtime_nodes.size()];

  for (auto i = 0; i < iara_runtime_nodes.size(); i++) {
    // per node
    iara_runtime_nodes.asSpan()[i].init();
    auto ptr = &iara_runtime_nodes.asSpan()[i];
    for (auto edge : ptr->output_fifos.asSpan()) {
      edge->init();
    }
  };
}
