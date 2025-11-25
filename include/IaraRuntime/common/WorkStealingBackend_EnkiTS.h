#ifndef IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_H
#define IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_H

// EnkiTS backend implementation for VirtualFIFO parallelism abstraction

// C++ interface (only available when compiling as C++)
#ifdef __cplusplus

#include "enkiTS/TaskScheduler.h"
#include <functional>
#include <mutex>
#include <vector>

namespace iara_enkits {
// Global EnkiTS scheduler instance
extern enki::TaskScheduler *g_scheduler;

// Pool of tasks for cleanup after completion
extern std::vector<enki::TaskSet *> g_task_pool;
extern std::mutex g_task_pool_mutex;
} // namespace iara_enkits

// Task submission using EnkiTS TaskSet
// Usage: iara_submit_task([=]() { /* task body */ });
template <typename Func> inline void iara_submit_task(Func &&func) {
  // Create a TaskSet that executes the lambda once
  auto *task = new enki::TaskSet(
      1,
      [func = std::forward<Func>(func)](enki::TaskSetPartition range,
                                        uint32_t threadnum) { func(); });

  iara_enkits::g_scheduler->AddTaskSetToPipe(task);

  // Store task for later cleanup
  {
    std::lock_guard<std::mutex> lock(iara_enkits::g_task_pool_mutex);
    iara_enkits::g_task_pool.push_back(task);
  }
}

// Parallel region execution - with EnkiTS, all threads are already active
template <typename Func> inline void iara_parallel_exec(Func &&func) { func(); }

// Single thread execution - in EnkiTS context, this is just direct execution
template <typename Func> inline void iara_single_exec(Func &&func) { func(); }

// Parallel region with single thread (common pattern)
template <typename Func> inline void iara_parallel_single(Func &&func) {
  func();
}

// Task synchronization: wait for all pending tasks
inline void iara_task_wait() {
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

// Get number of threads
inline int iara_get_num_threads() {
  return iara_enkits::g_scheduler->GetConfig().numTaskThreadsToCreate +
         1; // +1 for main thread
}

// Initialize parallelism runtime (called once at startup)
inline void iara_parallelism_init() {
  if (!iara_enkits::g_scheduler) {
    iara_enkits::g_scheduler = new enki::TaskScheduler();
    iara_enkits::g_scheduler->Initialize();
  }
}

// Shutdown parallelism runtime (called once at cleanup)
inline void iara_parallelism_shutdown() {
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

#endif // __cplusplus

// C-friendly interface for submitting function pointer tasks
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*iara_task_func)(void*);

void iara_submit_task_c(iara_task_func func, void* data);
void iara_task_wait_c(void);
void iara_parallelism_init_c(void);
void iara_parallelism_shutdown_c(void);

#ifdef __cplusplus
}
#endif

#endif // IARA_RUNTIME_WORKSTEALINGBACKEND_ENKITS_H
