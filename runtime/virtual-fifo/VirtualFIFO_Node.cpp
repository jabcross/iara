#include "Iara/Util/Span.h"
#include "IaraRuntime/virtual-fifo/Chunk.h"
#include "IaraRuntime/virtual-fifo/KeyedSemaphore.h"
#include "IaraRuntime/virtual-fifo/SDFSemaphores.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>

void VirtualFIFO_Node::consume(i64 seq,
                               Chunk chunk,
                               i64 arg_idx,
                               i64 offset_partial) {
  auto f = NormalSemaphore::FirstArgs{this};
  auto e =
      NormalSemaphore::EveryTimeArgs{.data = std::move(chunk),
                                     .arg_idx = arg_idx,
                                     .first_of_firing = (offset_partial == 0)};

  bool may_fire = false;
  Span<Chunk> args;

  auto l = NormalSemaphore::LastArgs{&may_fire, &args};

  // fprintf(stderr,
  //         "Consume %ld of %s[%ld] (chunk %ld:%ld)\n",
  //         seq,
  //         name,
  //         arg_idx,
  //         chunk.virtual_offset,
  //         chunk.virtual_offset + chunk.data_size);
  // fflush(stderr);

  // will set may_fire to true if it's the last dependency.
  sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, info.input_bytes + info.needs_priming, f, e, l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::dealloc(i64 current_buffer_size,
                               i64 first_buffer_size,
                               i64 next_buffer_sizes,
                               Chunk chunk) {
  i64 seq = 0;
  i64 off = chunk.virtual_offset;
  if (chunk.virtual_offset >= first_buffer_size) {
    auto pair =
        lldiv(chunk.virtual_offset - first_buffer_size, next_buffer_sizes);
    seq = pair.quot + 1;
    off = pair.rem;
  }
  auto f = NormalSemaphore::FirstArgs{this};
  auto e = NormalSemaphore::EveryTimeArgs{
      .data = chunk, .arg_idx = 0, .first_of_firing = (off == 0)};

  bool may_fire = false;
  Span<Chunk> args = {nullptr, 0};

  auto l = NormalSemaphore::LastArgs{&may_fire, &args};

  sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, current_buffer_size, f, e, l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::prime(i64 seq) {

  // fprintf(stderr,
  //         "Priming %ld of %s (total firings %ld)\n",
  //         seq,
  //         name,
  //         info.total_iter_firings);
  // fflush(stderr);

  // Schedule the allocation of the memory, if needed
  for (auto fifo : input_fifos.asSpan()) {
    auto [b, e] = fifo->firingOfConsToVirtualOffsetRange(seq);
    // fprintf(stderr,
    //         "virtual offset of firing %ld of node %s is %ld:%ld\n",
    //         seq,
    //         name,
    //         b,
    //         e);
    // fflush(stderr);
    auto block = fifo->getSingleBlockNumberFromVirtualOffset(b);
    // fprintf(stderr, "block number of range %ld:%ld is %ld\n", b, e, block);
    // fflush(stderr);

#pragma omp task
    {
      // fprintf(stderr, "firing %ld of node %s calling ensureAlloc\n", seq,
      // name); fflush(stderr);
      fifo->alloc_node->ensureAlloc(block);
    }
  }

  auto f = NormalSemaphore::FirstArgs{this};
  auto e = NormalSemaphore::EveryTimeArgs{
      .data = Chunk::make_empty(), .arg_idx = -1, .first_of_firing = 0};

  bool may_fire = false;
  Span<Chunk> args;

  auto l = NormalSemaphore::LastArgs{&may_fire, &args};
  sema_variant.normal->semaphore.arrive(
      seq, 1, info.input_bytes + info.needs_priming, f, e, l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::fire(i64 seq, Span<Chunk> args) {
  auto ptr = args.ptr;
  // fprintf(stderr, "Firing %ld of %s\n", seq, name);
  // fflush(stderr);
  assert((i64)args.extents == info.num_inputs);
#pragma omp task
  {
    wrapper(seq, args.ptr);
  }

#pragma omp taskwait
  // output_fifos is empty if it's a dealloc node.
  assert(output_fifos.extents == 0 || (output_fifos.extents == args.extents));
  for (size_t i = 0; i < output_fifos.extents; i++) {
    output_fifos.ptr[i]->push(args.ptr[i]);
  }
  // fprintf(stderr, "freeing %#016lx\n", (size_t)ptr);
  // fflush(stderr);
  free(ptr);
}

void VirtualFIFO_Node::fireAlloc(i64 seq) {
  auto alloc_fifo = *output_fifos.ptr;

  // fprintf(stderr, "fireAlloc %ld of %s\n", seq, name);
  // fflush(stderr);

  if (seq == 0) {
    i64 virtual_offset = 0;
    i64 block_size = alloc_fifo->info.block_size_with_delays;
    i64 total_delays =
        alloc_fifo->info.delay_offset + alloc_fifo->info.delay_size;
    auto chunk = Chunk::allocate(block_size, virtual_offset);
    // fprintf(stderr,
    //         "alloc %#016lx of size %ld\n",
    //         (size_t)chunk.allocated,
    //         block_size);
    // fflush(stderr);
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
    auto chunk = Chunk::allocate(block_size, virtual_offset);
    alloc_fifo->push(chunk);
    // fprintf(stderr,
    //         "alloc %#016lx of size %ld\n",
    //         (size_t)chunk.allocated,
    //         block_size);
    // fflush(stderr);
  }
}

// Uses the alloc node's semaphore to schedule the allocation.
void VirtualFIFO_Node::ensureAlloc(i64 firing) {

  // Placeholder: figure out a way to keep which blocks were already allocated
  // without an infinitely growing map.

  // All first allocs run on init.
  if (firing == 0)
    return;

  // fprintf(stderr,
  //         "ensureAlloc: firing %ld of %s (expected dependents = %ld)\n",
  //         firing,
  //         name,
  //         info.total_iter_firings);

  // fflush(stderr);

  bool may_alloc = false;

  VirtualFIFO_Node::AllocSemaphore::FirstArgs f{&may_alloc};
  VirtualFIFO_Node::AllocSemaphore::EveryTimeArgs e{};
  VirtualFIFO_Node::AllocSemaphore::LastArgs l{};

  sema_variant.alloc->semaphore.arrive(
      firing, 1, info.total_iter_firings, f, e, l);

  if (may_alloc) {
    fireAlloc(firing);
  }
}

void VirtualFIFO_Node::init() {
  if (info.isAlloc()) {
    sema_variant.alloc = new AllocSemaphore{};
  } else {
    sema_variant.normal = new NormalSemaphore{};
  }
  // todo: free this later
};
