#include "Iara/Util/Span.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/KeyedSemaphore.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include <cassert>
#include <cstdlib>

struct SDF_OoO_Node::NormalSemaphore {

  struct FirstArgs {
    SDF_OoO_Node *_this;
  };
  struct EveryTimeArgs {
    Chunk data;
    i64 arg_idx;
    bool first_of_firing;
  };

  struct LastArgs {
    i64 cons_seq;
    SDF_OoO_Node *_this;
  };

  static void first_time_func(FirstArgs &f_args, Span<Chunk> &kernel_args) {
    // Init args vector with appropriate size.
    kernel_args.extents = f_args._this->info.num_inputs;
    kernel_args.ptr = (Chunk *)calloc(sizeof(Chunk), kernel_args.extents);
  };

  static void every_time_func(EveryTimeArgs &et_args,
                              Span<Chunk> &kernel_args) {
    auto &[new_chunk, idx, first] = et_args;

    if (new_chunk.is_empty()) {
      return;
    }

    auto &old_chunk = kernel_args.ptr[idx];
    assert(kernel_args.size() > (size_t)idx);
    // only the first chunk of a firing contains the right pointer.
    if (first) {
      old_chunk = new_chunk;
    }
  };

  static void last_time_func(LastArgs &l_args, Span<Chunk> &kernel_args) {
    l_args._this->fire(l_args.cons_seq, kernel_args);
  };

  using Semaphore = keyed_semaphore::KeyedSemaphore<Span<Chunk>,
                                                    FirstArgs,
                                                    EveryTimeArgs,
                                                    LastArgs,
                                                    first_time_func,
                                                    every_time_func,
                                                    last_time_func>;

  Semaphore semaphore;
};

struct SDF_OoO_Node::AllocSemaphore {

  struct EntryData {};

  struct FirstArgs {
    SDF_OoO_Node *_this;
    i64 seq;
  };

  struct EveryTimeArgs {};

  struct LastArgs {};

  static void first_time_func(FirstArgs &f_args, EntryData &kernel_args) {
    // Init args vector with appropriate size.
    f_args._this->fireAlloc(f_args.seq);
  };

  static void every_time_func(EveryTimeArgs &et_args, EntryData &kernel_args){};

  static void last_time_func(LastArgs &l_args, EntryData &kernel_args){};

  using Semaphore = keyed_semaphore::KeyedSemaphore<EntryData,
                                                    FirstArgs,
                                                    EveryTimeArgs,
                                                    LastArgs,
                                                    first_time_func,
                                                    every_time_func,
                                                    last_time_func>;

  Semaphore semaphore;
};

void SDF_OoO_Node::consume(i64 seq,
                           Chunk chunk,
                           i64 arg_idx,
                           i64 offset_partial) {
  auto f = NormalSemaphore::FirstArgs{this};
  auto e =
      NormalSemaphore::EveryTimeArgs{.data = std::move(chunk),
                                     .arg_idx = arg_idx,
                                     .first_of_firing = (offset_partial == 0)};
  auto l = NormalSemaphore::LastArgs{seq, this};
  sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, info.input_bytes + info.needs_priming, f, e, l);
}

void SDF_OoO_Node::dealloc(i64 current_buffer_size,
                           i64 first_buffer_size,
                           i64 next_buffer_sizes,
                           Chunk chunk) {
  i64 seq = 0;
  i64 off = chunk.virtual_offset;
  if (chunk.virtual_offset > first_buffer_size) {
    auto pair =
        lldiv(chunk.virtual_offset - first_buffer_size, next_buffer_sizes);
    seq = pair.quot + 1;
    off = pair.rem;
  }
  auto f = NormalSemaphore::FirstArgs{this};
  auto e = NormalSemaphore::EveryTimeArgs{
      .data = chunk, .arg_idx = 0, .first_of_firing = (off == 0)};
  auto l = NormalSemaphore::LastArgs{seq, this};
  sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, current_buffer_size, f, e, l);
}

void SDF_OoO_Node::prime(i64 seq) {

  // Schedule the allocation of the memory, if needed
  for (auto fifo : input_fifos.asSpan()) {
    auto [b, e] = fifo->firingToVirtualOffsetRange(seq);
    auto block = fifo->getSingleBlockNumberFromVirtualOffset(b);
    fifo->alloc_node->ensureAlloc(block);
  }

  auto f = NormalSemaphore::FirstArgs{this};
  auto e = NormalSemaphore::EveryTimeArgs{
      .data = Chunk::make_empty(), .arg_idx = -1, .first_of_firing = 0};
  auto l = NormalSemaphore::LastArgs{seq, this};
  sema_variant.normal->semaphore.arrive(
      seq, 1, info.input_bytes + info.needs_priming, f, e, l);
}

void SDF_OoO_Node::fire(i64 seq, Span<Chunk> args) {
  assert((i64)args.extents == info.num_inputs);
  wrapper(seq, args.ptr);
  assert(output_fifos.extents == 0 || (output_fifos.extents == args.extents));
  for (size_t i = 0; i < output_fifos.extents; i++) {
    output_fifos.ptr[i]->push(args.ptr[i]);
  }
  free(args.ptr);
}

void SDF_OoO_Node::fireAlloc(i64 seq) {
  auto alloc_fifo = *output_fifos.ptr;

  if (seq == 0) {
    i64 virtual_offset = 0;
    i64 block_size = alloc_fifo->info.block_size_with_delays;
    i64 total_delays =
        alloc_fifo->info.delay_offset + alloc_fifo->info.delay_size;
    auto chunk = Chunk::allocate(block_size, virtual_offset);
    if (total_delays > 0) {
      auto delays = chunk.take_front(total_delays);
      alloc_fifo->propagate_delays(delays);
      virtual_offset += total_delays;
    }
    alloc_fifo->push(chunk);
  } else {
    i64 virtual_offset = ((seq - 1) * alloc_fifo->info.block_size_no_delays +
                          alloc_fifo->info.block_size_with_delays);
    i64 block_size = alloc_fifo->info.block_size_no_delays;

    alloc_fifo->push(Chunk::allocate(block_size, virtual_offset));
  }
}

// Uses the alloc node's semaphore to schedule the allocation.
void SDF_OoO_Node::ensureAlloc(i64 firing) {
  SDF_OoO_Node::AllocSemaphore::FirstArgs f{this, firing};
  SDF_OoO_Node::AllocSemaphore::EveryTimeArgs e{};
  SDF_OoO_Node::AllocSemaphore::LastArgs l{};

  // Placeholder: figure out a way to keep which blocks were already allocated
  // without an infinitely growing map.

  // All first allocs run on init.
  if (firing == 0)
    return;

  sema_variant.alloc->semaphore.arrive(
      info.id, 1, info.total_iter_firings, f, e, l);
}

void SDF_OoO_Node::init() {
  if (info.isAlloc()) {
    sema_variant.alloc = new AllocSemaphore{};
  } else {
    sema_variant.normal = new NormalSemaphore{};
  }
  // todo: free this later
};
