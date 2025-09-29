#ifndef IARARUNTIME_COMMON_SCHEDULER_H
#define IARARUNTIME_COMMON_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
  #define EXTERNC extern "C"
#else
  #define EXTERNC
#endif

// Takes in a function that will be executed after scheduler setup.
// Must call `iara_runtime_init` once and `iara_runtime_run_iteration`.

EXTERNC void iara_runtime_exec(void (*)());
EXTERNC void iara_runtime_init();
EXTERNC void iara_runtime_run_iteration(int64_t graph_iteration);

// Waits for all tasks to finish.
EXTERNC void iara_runtime_wait();

#endif