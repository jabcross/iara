#ifndef IARARUNTIME_COMMON_SCHEDULER_H
#define IARARUNTIME_COMMON_SCHEDULER_H

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
  #define EXTERNC extern "C"
  #include "IaraRuntime/common/IO.h"
#else
  #define EXTERNC
#endif

// Takes in a function that will be executed after scheduler setup.
// Must call `iara_runtime_init` once and `iara_runtime_run_iteration`.

EXTERNC void iara_runtime_exec(void (*)());

// Sets up I/O sources and sinks for the runtime.
// num_io_ports: total number of I/O ports (sources + sinks)
// Takes variadic IaraSource* and IaraSink* arguments matching the in/inout/out
// ports. Must be called before iara_runtime_init, if at all.
EXTERNC void iara_runtime_set_io(int num_io_ports, ...);

// Initializes the runtime.
EXTERNC void iara_runtime_init();

EXTERNC void iara_runtime_run_iteration(int64_t graph_iteration);

// Waits for all tasks to finish.
EXTERNC void iara_runtime_wait();

// Shuts down the runtime scheduler.
EXTERNC void iara_runtime_shutdown();

#endif