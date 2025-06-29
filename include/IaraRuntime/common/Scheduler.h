#ifndef IARARUNTIME_COMMON_SCHEDULER_H
#define IARARUNTIME_COMMON_SCHEDULER_H

#include <cstdint>

#ifdef __cplusplus
  #define EXTERNC extern "C"
#else
  #define EXTERNC
#endif

EXTERNC void iara_runtime_exec(void (*)());
EXTERNC void iara_runtime_init();
EXTERNC void iara_runtime_run_iteration(int64_t graph_iteration);

#endif