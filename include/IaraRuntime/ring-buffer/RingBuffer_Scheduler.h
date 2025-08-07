#ifndef IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H
#define IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H

#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"

struct VirtualFIFO_RuntimeData {
  std::span<VirtualFIFO_Node> node_infos;
  std::span<VirtualFIFO_Edge> fifo_infos;
};

// Create all data structures
extern "C" void iara_runtime_init();

// run single iteration
extern "C" void iara_runtime_run_iteration(i64 graph_iteration);

// call instruction wrapper (delegates underlying runtime)
extern "C" void iara_runtime_exec(void (*exec)());

#endif // IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H