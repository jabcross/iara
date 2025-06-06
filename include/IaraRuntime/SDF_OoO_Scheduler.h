#ifndef IARA_RUNTIME_SDF_OOO_SCHEDULER_H
#define IARA_RUNTIME_SDF_OOO_SCHEDULER_H

#include "IaraRuntime/SDF_OoO_FIFO.h"

struct SDF_OoO_Scheduler {};

struct SDF_OoO_RuntimeData {
  Span<SDF_OoO_Node> node_infos;
  Span<SDF_OoO_FIFO> fifo_infos;
};

extern "C" void kickstart_alloc(SDF_OoO_Node *alloc);

#endif // IARA_RUNTIME_SDF_OOO_SCHEDULER_H