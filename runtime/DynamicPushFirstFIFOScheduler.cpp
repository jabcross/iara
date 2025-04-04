#include "IaraRuntime/DynamicPushFirstFIFOScheduler.h"
#include <alloca.h>
#include <cstddef>
#include <cstdlib>

extern "C" void iara_fire_node(NodeInfo *node) {
#pragma omp parallel for
  for (int seq = 0; seq < node->total_firings; seq++) {
    i64 num_args =
        node->num_pure_inputs + node->num_inouts + node->num_pure_outputs;
    byte **args = (byte **)alloca(num_args);

    // wait for inputs;

    for (int i = 0, e = node->num_pure_inputs + node->num_inouts; i < e; i++) {
#pragma omp task default(private)
      {
        auto &edge = node->input_edges[i];
        args[i] = edge.fifo->pop(edge.cons_rate, seq);
      }
    }

    // allocate space for pure outputs
    for (int i = node->num_pure_inputs + node->num_inouts; i < num_args; i++) {
      args[i] = (byte *)malloc(node->output_edges[i].prod_rate);
    }

    // wait for all buffers to be ready.
#pragma taskwait

    // fire
    node->kernel_wrapper(args);

    // Push outputs

    for (int i = node->num_pure_inputs; i < num_args; i++) {
#pragma omp task default(private) {
      auto &edge = node->output_edges[i - node->num_pure_inputs];
      edge.fifo->push(edge.prod_rate, seq, args[i]);
    }

    // free pure input memory

    for (int i = 0; i < node->num_pure_inputs; i++) {
      free(args[i]);
    }
  }
}