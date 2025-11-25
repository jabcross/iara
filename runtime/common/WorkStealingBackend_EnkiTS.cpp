// EnkiTS backend implementation - global variable definitions

#include "IaraRuntime/common/WorkStealingBackend_EnkiTS.h"

namespace iara_enkits {
  // Global EnkiTS scheduler instance
  enki::TaskScheduler* g_scheduler = nullptr;

  // Pool of tasks for cleanup after completion
  std::vector<enki::TaskSet*> g_task_pool;
  std::mutex g_task_pool_mutex;
}

// C-friendly interface implementations
extern "C" {

void iara_submit_task_c(iara_task_func func, void* data) {
  auto* task = new enki::TaskSet(1, [func, data](enki::TaskSetPartition range, uint32_t threadnum) {
    func(data);
  });

  iara_enkits::g_scheduler->AddTaskSetToPipe(task);

  // Store task for later cleanup
  {
    std::lock_guard<std::mutex> lock(iara_enkits::g_task_pool_mutex);
    iara_enkits::g_task_pool.push_back(task);
  }
}

void iara_task_wait_c(void) {
  // Copy task pool to local vector (to avoid holding mutex during wait)
  std::vector<enki::TaskSet*> tasks_to_wait;
  {
    std::lock_guard<std::mutex> lock(iara_enkits::g_task_pool_mutex);
    tasks_to_wait = iara_enkits::g_task_pool;
    iara_enkits::g_task_pool.clear();
  }

  // Wait for each task individually (without holding mutex)
  for (auto *task : tasks_to_wait) {
    iara_enkits::g_scheduler->WaitforTask(task);
  }

  // Clean up completed tasks
  for (auto *task : tasks_to_wait) {
    delete task;
  }
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
