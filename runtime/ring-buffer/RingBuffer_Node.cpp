#include "Iara/Util/Span.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>
#include <thread>

void RingBuffer_Node::init() {};

void freeWorkingMemory(Span<Chunk> working_memory) {
  free(working_memory.ptr[0].allocated);
  free(working_memory.ptr);
}

Span<Chunk> allocWorkingMemory(RingBuffer_Node *node) {

  size_t num_args = node->input_fifos.extents + node->output_fifos.extents;

  Chunk *args = (Chunk *)calloc(sizeof(Chunk), num_args);

  i8 *working_memory = (i8 *)malloc(node->info.working_memory_size);

  // fprintf(stderr, "Allocd memory at %#016lx\n", (size_t)working_memory);
  // fflush(stderr);

  i8 *ptr = working_memory;
  i64 arg_idx = 0;
  for (auto input : node->input_fifos.asSpan()) {
    args[arg_idx] = Chunk{working_memory, ptr, input->info.cons_rate};
    ptr += input->info.cons_rate_aligned;
    arg_idx++;
  }
  for (i64 i = node->info.num_inouts, e = node->output_fifos.extents; i < e;
       i++) {
    auto output = node->output_fifos.asSpan()[i];
    args[arg_idx] = Chunk{working_memory, ptr, output->info.prod_rate};
    ptr += output->info.prod_rate_aligned;
    arg_idx++;
  }
  assert(ptr - working_memory == node->info.working_memory_size);

  return Span<Chunk>{args, num_args};
}

void popInputs(RingBuffer_Node *node, Span<Chunk> working_memory) {
#pragma omp taskgroup
  {
    for (size_t i = 0; i < node->input_fifos.extents; i++) {
      auto edge_ptr = node->input_fifos.ptr[i];
      Chunk c = working_memory.ptr[i];
#pragma omp task firstprivate(edge_ptr, c)
      {
        while (!edge_ptr->tryPop(c)) {
#pragma omp taskyield
          std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
      }
    }
  }
}

void pushOutputs(RingBuffer_Node *node, Span<Chunk> working_memory) {
#pragma omp taskgroup
  {
    for (size_t output_fifo_index = 0,
                num_output_fifos = node->output_fifos.extents,
                arg_index = node->info.num_ins;
         output_fifo_index < num_output_fifos;
         output_fifo_index++, arg_index++) {
      auto edge_ptr = node->output_fifos.ptr[output_fifo_index];
      Chunk chunk = working_memory.ptr[arg_index];
#pragma omp task firstprivate(edge_ptr) firstprivate(chunk)
      {
        edge_ptr->push(chunk);
      }
    }
  }
}

void RingBuffer_Node::run_iteration() {
  // inputs
  for (i64 firing = 0; firing < info.total_iter_firings; firing++) {
    auto working_memory = allocWorkingMemory(this);
    // fprintf(stderr,
    //         "Start firing %ld/%ld of %ld\n",
    //         firing + 1,
    //         info.total_iter_firings,
    //         info.id);
    // fflush(stderr);
    popInputs(this, working_memory);
    // fprintf(stderr,
    //         "Running firing %ld/%ld of %ld\n",
    //         firing + 1,
    //         info.total_iter_firings,
    //         info.id);
    // fflush(stderr);
    wrapper(working_memory.ptr);
    pushOutputs(this, working_memory);
    // fprintf(stderr,
    //         "End firing %ld/%ld of %ld\n",
    //         firing + 1,
    //         info.total_iter_firings,
    //         info.id);
    // fflush(stderr);
    freeWorkingMemory(working_memory);
  }
}
