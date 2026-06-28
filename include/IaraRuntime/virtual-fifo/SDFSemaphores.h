#ifndef IARARUNTIME_VIRTUALFIFO_SDFSEMAPHORES_H
#define IARARUNTIME_VIRTUALFIFO_SDFSEMAPHORES_H

#include "IaraRuntime/virtual-fifo/KeyedSemaphore.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <span>

// Max kernel args stored inline in a ring slot (ring variant only). Nodes with
// more args fall back to a heap-allocated arg array (rare; SIFT max is 11).
#ifndef IARA_SEMAPHORE_INLINE_ARGS
#define IARA_SEMAPHORE_INLINE_ARGS 16
#endif

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

#ifdef IARA_RING_SEMAPHORE
  // Ring variant: the kernel arg array lives INLINE in the semaphore slot, so
  // its lifetime is the slot's lifetime (freed by release() after the kernel
  // runs). No per-firing calloc on the common path (num_args <= K).
  struct ArgStore {
    static constexpr int K = IARA_SEMAPHORE_INLINE_ARGS;
    VirtualFIFO_Chunk inline_buf[K];
    VirtualFIFO_Chunk *heap = nullptr; // used only when n > K
    int n = 0;
    VirtualFIFO_Chunk *data() { return heap ? heap : inline_buf; }
  };

  static void first_time_func(FirstArgs &f_args, ArgStore &store) {
    int n = (int)f_args._this->runtime_info.num_args;
    store.n = n;
    if (n > ArgStore::K) {
      store.heap =
          (VirtualFIFO_Chunk *)calloc(n, sizeof(VirtualFIFO_Chunk));
    } else {
      store.heap = nullptr;
      for (int i = 0; i < n; i++)
        store.inline_buf[i] = VirtualFIFO_Chunk::make_empty();
    }
  }

  static void every_time_func(EveryTimeArgs &et_args, ArgStore &store) {
    auto &[new_chunk, idx, first] = et_args;
    if (new_chunk.is_empty())
      return;
    assert(store.n > (int)idx);
    // only the first chunk of a firing contains the right pointer.
    if (first)
      store.data()[idx] = new_chunk;
  }

  static void last_time_func(LastArgs &l_args, ArgStore &store) {
    *l_args.may_fire = true;
    *l_args.args = std::span<VirtualFIFO_Chunk>(store.data(), store.n);
  }

  static void cleanup_func(ArgStore &store) {
    if (store.heap) {
      free(store.heap);
      store.heap = nullptr;
    }
    store.n = 0;
  }

  using Semaphore = keyed_semaphore::KeyedSemaphoreRing<
      keyed_semaphore::ParallelHashMap,
      ArgStore,
      FirstArgs,
      EveryTimeArgs,
      LastArgs,
      first_time_func,
      every_time_func,
      last_time_func,
      cleanup_func>;

#else // map variant (default): kernel arg array is a per-firing calloc, freed
      // by fire() via free(args.data()).

  static void first_time_func(FirstArgs &f_args,
                              std::span<VirtualFIFO_Chunk> &kernel_args) {
    // Init args vector with appropriate size.

    auto p_kernel_args = &kernel_args;

    size_t size = f_args._this->runtime_info.num_args;
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
#endif

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

#ifdef IARA_RING_SEMAPHORE
  static void cleanup_func(EntryData &kernel_args) {};

  using Semaphore = keyed_semaphore::KeyedSemaphoreRing<
      keyed_semaphore::ParallelHashMap,
      EntryData,
      FirstArgs,
      EveryTimeArgs,
      LastArgs,
      first_time_func,
      every_time_func,
      last_time_func,
      cleanup_func>;
#else
  using Semaphore =
      keyed_semaphore::KeyedSemaphore<keyed_semaphore::ParallelHashMap,
                                      EntryData,
                                      FirstArgs,
                                      EveryTimeArgs,
                                      LastArgs,
                                      first_time_func,
                                      every_time_func,
                                      last_time_func>;
#endif

  Semaphore semaphore;
};
#endif