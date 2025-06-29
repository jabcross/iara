#ifndef IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H
#define IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H

#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"

struct VirtualFIFO_RuntimeData {
  Span<VirtualFIFO_Node> node_infos;
  Span<VirtualFIFO_Edge> fifo_infos;
};

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(i64 graph_iteration);
extern "C" void iara_runtime_exec(void (*)());

#endif // IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H