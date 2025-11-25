#ifndef IARA_RUNTIME_WORKSTEALINGBACKEND_OMP_H
#define IARA_RUNTIME_WORKSTEALINGBACKEND_OMP_H

// OpenMP backend implementation for VirtualFIFO parallelism abstraction

#include <omp.h>
#include <functional>

// Task submission: OpenMP uses pragmas, so we use lambda + pragma pattern
// Usage: iara_submit_task([=]() { /* task body */ });
template<typename Func>
inline void iara_submit_task(Func&& func) {
#pragma omp task
  func();
}

// Parallel region execution
// Usage: iara_parallel_exec([&]() { /* parallel code */ });
template<typename Func>
inline void iara_parallel_exec(Func&& func) {
#pragma omp parallel
  func();
}

// Single thread execution within parallel region
// Usage: iara_single_exec([&]() { /* code for single thread */ });
template<typename Func>
inline void iara_single_exec(Func&& func) {
#pragma omp single
  func();
}

// Parallel region with single thread (common pattern)
// Usage: iara_parallel_single([&]() { /* spawn tasks here */ });
template<typename Func>
inline void iara_parallel_single(Func&& func) {
#pragma omp parallel
  {
#pragma omp single
    func();
  }
}

// Task synchronization: wait for all pending tasks
inline void iara_task_wait() {
#pragma omp taskwait
}

// Get number of threads in current team
inline int iara_get_num_threads() {
  return omp_get_num_threads();
}

// Initialize parallelism runtime (called once at startup)
inline void iara_parallelism_init() {
  // OpenMP initializes automatically
}

// Shutdown parallelism runtime (called once at cleanup)
inline void iara_parallelism_shutdown() {
  // OpenMP cleans up automatically
}

#endif // IARA_RUNTIME_WORKSTEALINGBACKEND_OMP_H
