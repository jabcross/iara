#ifndef IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H
#define IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H

#include "IaraRuntime/common/Scheduler.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"

struct VirtualFIFO_RuntimeData {
  Span<VirtualFIFO_Node> node_infos;
  Span<VirtualFIFO_Edge> fifo_infos;
};

#endif // IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H