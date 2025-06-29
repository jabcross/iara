#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cassert>
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <llvm/Support/ErrorHandling.h>
#include <semaphore>
#include <thread>
#include <vector>

extern std::span<RingBuffer_Node> iara_runtime_nodes;
extern std::span<RingBuffer_Edge> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide
std::vector<std::thread> threads;

template <class F> void callBlockingFunctionFromOpenMPTask(F f) {
  // assuming this is called from an OpenMP Task
}

struct sema_wrapper {
  std::binary_semaphore sema;
  constexpr sema_wrapper() : sema(0) {};
  constexpr explicit sema_wrapper(auto count) : sema(count) {};
};

std::counting_semaphore<> iter_ended{0};

sema_wrapper *semas;

size_t getIndex(RingBuffer_Node &node) {
  return &node - &iara_runtime_nodes.front();
}

struct node_thread {};

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {
  for (auto i = 0; i < iara_runtime_nodes.size(); i++) {
    semas[i].sema.release();
  }
  for (auto i = 0; i < iara_runtime_nodes.size(); i++) {
    iter_ended.acquire();
  }
}

extern "C" void iara_runtime_init() {
  semas = new sema_wrapper[iara_runtime_nodes.size()];
  for (auto i = 0; i < iara_runtime_nodes.size(); i++) {
    iara_runtime_nodes[i].init();
    auto ptr = &iara_runtime_nodes[i];
    std::thread([ptr, i]() {
      for (auto edge : ptr->output_fifos.asSpan()) {
        edge->init();
      }
      while (true) {
        semas[i].sema.acquire();
        ptr->run_iteration();
        iter_ended.release();
      }
      llvm_unreachable("what");
    }).detach();
  }
}
