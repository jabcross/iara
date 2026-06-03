// EnkiTS backend implementation - global variable definitions

#include "IaraRuntime/common/WorkStealingBackend_EnkiTS.h"

namespace iara_enkits {
  // Global EnkiTS scheduler instance
  enki::TaskScheduler* g_scheduler = nullptr;

  // Pool of tasks for cleanup after completion
  std::vector<enki::TaskSet*> g_task_pool;
  std::mutex g_task_pool_mutex;

  // Outstanding (submitted, not yet finished) task count — see header.
  std::atomic<int64_t> g_outstanding{0};
}

// C-friendly interface implementations
extern "C" {

void iara_submit_task_c(iara_task_func func, void* data) {
  iara_enkits::g_outstanding.fetch_add(1, std::memory_order_relaxed);
  auto* task = new enki::TaskSet(1, [func, data](enki::TaskSetPartition range, uint32_t threadnum) {
    func(data);
    iara_enkits::g_outstanding.fetch_sub(1, std::memory_order_release);
  });

  iara_enkits::g_scheduler->AddTaskSetToPipe(task);

  // Store task for later cleanup
  {
    std::lock_guard<std::mutex> lock(iara_enkits::g_task_pool_mutex);
    iara_enkits::g_task_pool.push_back(task);
  }
}

void iara_task_wait_c(void) {
  // Tasks spawn further tasks dynamically (a fired node submits its downstream
  // consumers), so a one-shot snapshot of the pool only covers the first wave —
  // deeper tasks (e.g. a terminal sink) would be left running while this
  // returns, letting main() exit early with code 0 on a crash/incomplete graph.
  // Wait on the outstanding-task counter, which spans the whole transitive
  // closure; WaitforAll() runs available work so the count drains to zero.
  while (iara_enkits::g_outstanding.load(std::memory_order_acquire) > 0) {
    iara_enkits::g_scheduler->WaitforAll();
  }

  std::vector<enki::TaskSet *> done;
  {
    std::lock_guard<std::mutex> lock(iara_enkits::g_task_pool_mutex);
    done.swap(iara_enkits::g_task_pool);
  }
  for (auto *task : done)
    delete task;
}

void iara_parallelism_init_c(void) {
  if (!iara_enkits::g_scheduler) {
    iara_enkits::g_scheduler = new enki::TaskScheduler();
    iara_enkits::g_scheduler->Initialize();
  }
}

void iara_parallelism_shutdown_c(void) {
  if (iara_enkits::g_scheduler) {
    iara_enkits::g_scheduler->WaitforAllAndShutdown();

    // Clean up any remaining tasks
    for (auto *task : iara_enkits::g_task_pool) {
      delete task;
    }
    iara_enkits::g_task_pool.clear();

    delete iara_enkits::g_scheduler;
    iara_enkits::g_scheduler = nullptr;
  }
}

} // extern "C"
