#ifndef IARA_RUNTIME_WORKSTEALINGBACKEND_SEQUENTIAL_H
#define IARA_RUNTIME_WORKSTEALINGBACKEND_SEQUENTIAL_H

// Sequential (no parallelism) backend implementation for VirtualFIFO parallelism abstraction

#include <functional>

// Task submission: execute immediately
template<typename Func>
inline void iara_submit_task(Func&& func) {
  func();
}

// Parallel region: execute sequentially
template<typename Func>
inline void iara_parallel_exec(Func&& func) {
  func();
}

// Single thread: execute immediately
template<typename Func>
inline void iara_single_exec(Func&& func) {
  func();
}

// Parallel single: execute immediately
template<typename Func>
inline void iara_parallel_single(Func&& func) {
  func();
}

// Task synchronization: no-op
inline void iara_task_wait() {
  // No tasks to wait for
}

// Get number of threads: always 1
inline int iara_get_num_threads() {
  return 1;
}

// Initialize parallelism runtime: no-op
inline void iara_parallelism_init() {
  // Nothing to initialize
}

// Shutdown parallelism runtime: no-op
inline void iara_parallelism_shutdown() {
  // Nothing to clean up
}

#endif // IARA_RUNTIME_WORKSTEALINGBACKEND_SEQUENTIAL_H
