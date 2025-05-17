#ifndef IARA_RUNTIME_DYNAMIC_PUSH_FIRST_FIFO_SCHEDULER_H
#define IARA_RUNTIME_DYNAMIC_PUSH_FIRST_FIFO_SCHEDULER_H

#include "IaraRuntime/DynamicPushFirstFIFO.h"
#include <cstdint>

struct EdgeInfo {
  i64 prod_rate;
  i64 cons_rate;
  i64 delay_size;
  byte *initial_delays;
  DynamicPushFirstFifo *fifo;
};

struct NodeInfo {
  i64 total_firings;
  i64 num_pure_inputs;
  i64 num_inouts;
  i64 num_pure_outputs;
  EdgeInfo **input_edges;
  EdgeInfo **output_edges;

  void (*kernel_wrapper)(void *args);
};

void fire_node(NodeInfo *);

#endif // IARA_RUNTIME_DYNAMIC_PUSH_FIRST_FIFO_SCHEDULER_H