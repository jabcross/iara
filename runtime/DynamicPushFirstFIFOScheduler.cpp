#include "IaraRuntime/DynamicPushFirstFIFOScheduler.h"
#include <alloca.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <omp.h>

extern "C" void iara_fire_node(NodeInfo *node) {
#pragma omp parallel for
  for (int seq = 0; seq < node->total_firings; seq++) {
    printf("seq:%d, node:%ld, omp thread:%d\n", seq, (i64)node,
           omp_get_thread_num());
    i64 num_args =
        node->num_pure_inputs + node->num_inouts + node->num_pure_outputs;
    byte **args = (byte **)alloca(num_args);

    // wait for inputs;

    for (int i = 0, e = node->num_pure_inputs + node->num_inouts; i < e; i++) {
#pragma omp task default(private) shared(node, args)
      {
        auto edge = node->input_edges[i];
        args[i] = edge->fifo->pop(edge->cons_rate, seq);
      }
    }

    // allocate space for pure outputs
    for (int i = node->num_pure_inputs + node->num_inouts; i < num_args; i++) {
      args[i] = (byte *)malloc(node->output_edges[i]->prod_rate);
    }

    // wait for all buffers to be ready.
#pragma omp taskwait

    // fire
    node->kernel_wrapper(args);

    // Push outputs

    for (int i = node->num_pure_inputs; i < num_args; i++) {
#pragma omp task default(private) shared(node, args)
      {
        EdgeInfo *edge = node->output_edges[i - node->num_pure_inputs];
        edge->fifo->push(edge->prod_rate, seq, args[i]);
      }
    }

#pragma omp taskwait

    // free pure input memory

    for (int i = 0; i < node->num_pure_inputs; i++) {
      free(args[i]);
    }
  }
}
