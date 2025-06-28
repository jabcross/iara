#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

extern std::span<RingBuffer_Node> iara_runtime_nodes;
extern std::span<RingBuffer_Edge> iara_runtime_edges;

i64 iara_runtime_num_threads = 0; // 0 = let openmp decide

std::vector<std::thread> jobs;

extern "C" void iara_runtime_run_iteration(i64 graph_iteration) {
  jobs.reserve(iara_runtime_nodes.size());

  for (auto &node : iara_runtime_nodes) {
    auto ptr = &node;
    jobs.push_back(std::thread([ptr]() {
      static bool pushed_delays = false;

      if (!pushed_delays) {
        for (auto edge : ptr->output_fifos.asSpan()) {
          edge->init();
        }
        pushed_delays = true;
      }

      ptr->run_iteration();
      fprintf(stderr,
              "Node %ld done with %ld firings\n",
              ptr->info.id,
              ptr->info.total_iter_firings);
      fflush(stderr);
    }));
  }

  for (auto &job : jobs) {
    job.join();
  }
}

extern "C" void iara_runtime_init() {
  for (auto &node : iara_runtime_nodes) {
    node.init();
  }
  for (auto &edge : iara_runtime_edges) {
    edge.init();
  }
}
