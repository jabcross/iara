#include "IaraRuntime/ring-buffer/RingBuffer_Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>
#include <thread>

// #define IARA_DEBUGPRINT

#ifdef IARA_DEBUGPRINT
extern std::mutex debug_mutex;
#endif

void RingBuffer_Node::init() {};

// working_memory is a vector of contiguous chunks. it's ok to free the first.
void freeWorkingMemory(std::vector<RingBuffer_Chunk> &&working_memory) {
  assert(working_memory.size() >= 1);
  free(working_memory[0].allocated);
}

std::vector<RingBuffer_Chunk> allocWorkingMemory(RingBuffer_Node *node) {

  size_t num_args = node->input_fifos.size() + node->output_fifos.size();

  std::vector<RingBuffer_Chunk> args;
  args.reserve(num_args);

  i8 *working_memory = (i8 *)malloc(node->info.working_memory_size);

  // fprintf(stderr, "Allocd memory at %#016lx\n", (size_t)working_memory);
  // fflush(stderr);

  i8 *ptr = working_memory;
  for (auto input : node->input_fifos) {
    args.emplace_back(working_memory, ptr, input->info.cons_rate);
    ptr += input->info.cons_rate_aligned;
  }
  for (i64 i = node->info.num_inouts, e = node->output_fifos.size(); i < e;
       i++) {
    auto output = node->output_fifos[i];
    args.emplace_back(working_memory, ptr, output->info.prod_rate);
    ptr += output->info.prod_rate_aligned;
  }
  assert(ptr - working_memory == node->info.working_memory_size);

  return args;
}

void popInputs(RingBuffer_Node *node,
               std::span<RingBuffer_Chunk> working_memory) {
#ifndef IARA_DISABLE_OMP
  #pragma omp taskgroup
#endif

  {
    for (size_t i = 0; i < node->input_fifos.size(); i++) {
      auto edge_ptr = node->input_fifos[i];
      RingBuffer_Chunk c = working_memory[i];
#ifndef IARA_DISABLE_OMP
  #pragma omp task firstprivate(edge_ptr, c)
#endif
      {
        while (!edge_ptr->tryPop(c)) {
#ifndef IARA_DISABLE_OMP
  #pragma omp taskyield
#endif

          std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
      }
    }
  }
}

void pushOutputs(RingBuffer_Node *node,
                 std::span<RingBuffer_Chunk> working_memory) {
#ifndef IARA_DISABLE_OMP
  #pragma omp taskgroup
#endif
  {
    for (size_t output_fifo_index = 0,
                num_output_fifos = node->output_fifos.size(),
                arg_index = node->info.num_ins;
         output_fifo_index < num_output_fifos;
         output_fifo_index++, arg_index++) {
      auto edge_ptr = node->output_fifos[output_fifo_index];
      RingBuffer_Chunk chunk = working_memory[arg_index];
#ifndef IARA_DISABLE_OMP
  #pragma omp task firstprivate(edge_ptr) firstprivate(chunk)
#endif

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
#ifdef IARA_DEBUGPRINT
    debug_mutex.lock();
    fprintf(stderr,
            "Start firing %ld/%ld of %ld\n",
            firing + 1,
            info.total_iter_firings,
            info.id);
    fflush(stderr);
    debug_mutex.unlock();
#endif

    popInputs(this, working_memory);

#ifdef IARA_DEBUGPRINT
    debug_mutex.lock();
    // fprintf(stderr,
    //         "Running firing %ld/%ld of %ld\n",
    //         firing + 1,
    //         info.total_iter_firings,
    //         info.id);
    // fflush(stderr);
    debug_mutex.unlock();
#endif

    wrapper(&working_memory[0]);
    pushOutputs(this, working_memory);

#ifdef IARA_DEBUGPRINT
    debug_mutex.lock();
    // fprintf(stderr,
    //         "End firing %ld/%ld of %ld\n",
    //         firing + 1,
    //         info.total_iter_firings,
    //         info.id);
    // fflush(stderr);
    debug_mutex.unlock();
#endif

    freeWorkingMemory(std::move(working_memory));
  }
}
