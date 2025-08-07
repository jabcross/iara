#ifndef IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H
#define IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H

#include "IaraRuntime/common/Scheduler.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"

struct VirtualFIFO_RuntimeData {
  std::span<VirtualFIFO_Node> node_infos;
  std::span<VirtualFIFO_Edge> fifo_infos;
};

#endif // IARA_RUNTIME_VIRTUALFIFOSCHEDULER_H