#include "IaraRuntime/FIFOScheduler.h"
#include "Iara/Util/Range.h"
#include <atomic>

namespace iara_fifo_runtime {

void FIFO::push(i64 iter, byte *buffer) {
  // enforce correct order in FIFO
  int old = iter;
  while (!next_prod_iter.compare_exchange_strong(old, old + 1)) {
    next_prod_iter.wait(old);
    old = iter;
  }
  queue->enqueue_bulk(buffer, prod_rate);
}

void FIFO::pop(i64 iter, byte *buffer) {
  // enforce correct order in FIFO
  int old = iter;
  while (!next_cons_iter.compare_exchange_strong(old, old + 1)) {
    next_prod_iter.wait(old);
    old = iter;
  }
  queue->wait_dequeue_bulk(buffer, cons_rate);
}

extern "C" FIFO *iara_make_FIFO(i64 prod_rate, i64 cons_rate, i64 delay_size,
                                byte *delay_data) {
  auto rv = new FIFO();
  rv->prod_rate = prod_rate;
  rv->cons_rate = cons_rate;

  rv->queue = new moodycamel::BlockingConcurrentQueue<byte>();

  if (delay_size > 0)
    rv->queue->enqueue_bulk(delay_data, delay_size);
}

template <i64 num_args> void Node<num_args>::fire() {

  for (int firing = 0; firing < total_firings; firing++)

  {
    byte args[num_args];

    // Allocate space for inputs and inouts.
    for (i64 i = 0; i < num_incoming_fifos; i++) {
      args[i] = (byte *)calloc(incoming_fifos[i].prod_rate, 1);
    }

    // Allocate space for remaining (pure) outputs.
    for (i64 i = num_incoming_fifos; i < num_args; i++) {
      args[i] =
          (byte *)calloc(outgoing_fifos[i - num_incoming_fifos].prod_rate, 1);
    }

    // Wait for inputs
    for (int i = 0; i < num_incoming_fifos; i++) {
#pragma omp task priority(10)
      incoming_fifos[i].pop(&args[i]);
    }

#pragma omp taskwait

#pragma omp task priority(20)
    SpreadArgs<num_args>::type::call(kernel, args);

#pragma omp taskwait

    // Push outputs.
    for (i64 i = num_incoming_fifos, j = 0; i < num_args; i++, j++) {
#pragma omp task priority(0)
      outgoing_fifos[j].push(args[i]);
    }

#pragma omp taskwait

    // Free buffers.
    for (i64 i = 0, j = 0; i < num_args; i++, j++) {
      free(args[i]);
    }
  }
}

} // namespace iara_fifo_runtime
