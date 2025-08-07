#ifndef IARARUNTIME_VIRTUALFIFO_SDFSEMAPHORES_H
#define IARARUNTIME_VIRTUALFIFO_SDFSEMAPHORES_H

#include "IaraRuntime/virtual-fifo/KeyedSemaphore.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <span>

struct VirtualFIFO_NormalSemaphore {

  struct FirstArgs {
    VirtualFIFO_Node *_this;
  };
  struct EveryTimeArgs {
    VirtualFIFO_Chunk data;
    i64 arg_idx;
    bool first_of_firing;
  };

  struct LastArgs {
    bool *may_fire;
    std::span<VirtualFIFO_Chunk> *args;
  };

  static void first_time_func(FirstArgs &f_args,
                              std::span<VirtualFIFO_Chunk> &kernel_args) {
    // Init args vector with appropriate size.

    auto p_kernel_args = &kernel_args;

    size_t size = f_args._this->static_info.num_args;
    auto data = (VirtualFIFO_Chunk *)calloc(sizeof(VirtualFIFO_Chunk), size);
    *p_kernel_args = {data, size};

    // fprintf(stderr,
    //         "alloc args %#016lx of size %ld\n",
    //         (size_t)kernel_args.ptr,
    //         kernel_args.extents * sizeof(VirtualFIFO_Chunk));
    // fflush(stderr);
  };

  static void every_time_func(EveryTimeArgs &et_args,
                              std::span<VirtualFIFO_Chunk> &kernel_args) {
    auto &[new_chunk, idx, first] = et_args;

    if (new_chunk.is_empty()) {
      return;
    }

    auto &old_chunk = kernel_args[idx];
    assert(kernel_args.size() > (size_t)idx);
    // only the first chunk of a firing contains the right pointer.
    if (first) {
      old_chunk = {new_chunk};
    }
  };

  static void last_time_func(LastArgs &l_args,
                             std::span<VirtualFIFO_Chunk> &kernel_args) {
    *l_args.may_fire = true;
    *l_args.args = kernel_args;
  };

  using Semaphore =
      keyed_semaphore::KeyedSemaphore<keyed_semaphore::ParallelHashMap,
                                      std::span<VirtualFIFO_Chunk>,
                                      FirstArgs,
                                      EveryTimeArgs,
                                      LastArgs,
                                      first_time_func,
                                      every_time_func,
                                      last_time_func>;

  Semaphore semaphore;
};

struct VirtualFIFO_AllocSemaphore {

  struct EntryData {};

  struct FirstArgs {
    bool *may_alloc;
  };

  struct EveryTimeArgs {};

  struct LastArgs {};

  static void first_time_func(FirstArgs &f_args, EntryData &kernel_args) {
    // Init args vector with appropriate size.
    *f_args.may_alloc = true;
  };

  static void every_time_func(EveryTimeArgs &et_args, EntryData &kernel_args) {
  };

  static void last_time_func(LastArgs &l_args, EntryData &kernel_args) {};

  using Semaphore =
      keyed_semaphore::KeyedSemaphore<keyed_semaphore::ParallelHashMap,
                                      EntryData,
                                      FirstArgs,
                                      EveryTimeArgs,
                                      LastArgs,
                                      first_time_func,
                                      every_time_func,
                                      last_time_func>;

  Semaphore semaphore;
};
#endif