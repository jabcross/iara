// #include "IaraRuntime/virtual-fifo/KeyedSemaphore.h"
#include "IaraRuntime/virtual-fifo/SDFSemaphores.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>
#include <omp.h>

#ifdef IARA_DEBUGPRINT
  #include "IaraRuntime/util/DebugPrint.h"

  #include <mutex>
std::mutex debug_mutex;
#endif

void VirtualFIFO_Node::consume(i64 seq,
                               VirtualFIFO_Chunk chunk,
                               i64 arg_idx,
                               i64 offset_partial) {
  auto f = VirtualFIFO_NormalSemaphore::FirstArgs{this};
  auto e = VirtualFIFO_NormalSemaphore::EveryTimeArgs{
      .data = std::move(chunk),
      .arg_idx = arg_idx,
      .first_of_firing = (offset_partial == 0)};

  bool may_fire = false;
  std::span<VirtualFIFO_Chunk> args;

  auto l = VirtualFIFO_NormalSemaphore::LastArgs{&may_fire, &args};

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("Consume %ld of %s[%ld] (chunk %ld:%ld)\n",
                        seq,
                        codegen_info.name,
                        arg_idx,
                        chunk.virtual_offset,
                        chunk.virtual_offset + chunk.data_size);
#endif

  // will set may_fire to true if it's the last dependency.
  runtime_info.sema_variant.normal->semaphore.arrive(
      seq,
      chunk.data_size,
      static_info.arg_bytes + static_info.needs_priming,
      f,
      e,
      l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::dealloc(i64 current_buffer_size,
                               i64 first_buffer_size,
                               i64 next_buffer_sizes,
                               VirtualFIFO_Chunk chunk) {
  i64 seq = 0;
  i64 off = chunk.virtual_offset;
  if (chunk.virtual_offset >= first_buffer_size) {
    auto pair =
        lldiv(chunk.virtual_offset - first_buffer_size, next_buffer_sizes);
    seq = pair.quot + 1;
    off = pair.rem;
  }
  auto f = VirtualFIFO_NormalSemaphore::FirstArgs{this};
  auto e = VirtualFIFO_NormalSemaphore::EveryTimeArgs{
      .data = chunk, .arg_idx = 0, .first_of_firing = (off == 0)};

  bool may_fire = false;
  std::span<VirtualFIFO_Chunk> args = {};

  auto l = VirtualFIFO_NormalSemaphore::LastArgs{&may_fire, &args};

  runtime_info.sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, current_buffer_size, f, e, l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::prime(i64 seq) {

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("prime(): Priming %ld of %s (total firings %ld)\n",
                        seq,
                        codegen_info.name,
                        static_info.total_iter_firings);
#endif

  // Schedule the allocation of the memory, if needed
  for (auto fifo : codegen_info.input_fifos) {
    auto [b, e] = fifo->firingOfConsToVirtualOffsetRange(seq);

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor(
        "prime(): virtual offset of firing %ld of node %s is %ld:%ld\n",
        seq,
        codegen_info.name,
        b,
        e);
#endif

    auto block = fifo->getSingleBlockNumberFromVirtualOffset(b);

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor(
        "prime(): block number of range %ld:%ld is %ld\n", b, e, block);
#endif

#ifndef IARA_DISABLE_OMP
  #pragma omp task firstprivate(block)
#endif
    {

#ifdef IARA_DEBUGPRINT
      debugPrintThreadColor(
          "prime(): firing %ld of node %s calling ensureAlloc\n",
          seq,
          codegen_info.name);
#endif

      fifo->codegen_info.alloc_node->ensureAlloc(block);
    }
  }

  auto f = VirtualFIFO_NormalSemaphore::FirstArgs{this};
  auto e = VirtualFIFO_NormalSemaphore::EveryTimeArgs{
      .data = VirtualFIFO_Chunk::make_empty(),
      .arg_idx = -1,
      .first_of_firing = 0};

  bool may_fire = false;
  std::span<VirtualFIFO_Chunk> args;

  auto l = VirtualFIFO_NormalSemaphore::LastArgs{&may_fire, &args};
  runtime_info.sema_variant.normal->semaphore.arrive(
      seq, 1, static_info.arg_bytes + static_info.needs_priming, f, e, l);

  if (may_fire) {
    fire(seq, args);
  }
}

void VirtualFIFO_Node::fire(i64 seq, std::span<VirtualFIFO_Chunk> args) {

#ifdef IARA_DEBUGPRINT
  {
    auto offset = std::hash<std::thread::id>{};
    (std::this_thread::get_id());
    debugPrintThreadColor("fire(): Firing %ld of %s from thread %lu\n",
                          seq,
                          codegen_info.name,
                          offset);
  }
#endif
  assert((i64)args.size() == static_info.num_args);
  [[maybe_unused]] auto _this = this;

#ifndef IARA_DISABLE_OMP
  #ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("Omp is enabled, num threads =  %lu",
                        omp_get_num_threads());
  #endif
  #pragma omp task firstprivate(_this, args, seq)
#endif


  {

    codegen_info.wrapper(seq, args);
  }

#ifndef IARA_DISABLE_OMP
  #pragma omp taskwait
#endif

  // output_fifos is empty if it's a dealloc node.
  assert(codegen_info.output_fifos.size() == 0 ||
         (codegen_info.output_fifos.size() == args.size()));
  for (size_t i = 0; i < codegen_info.output_fifos.size(); i++) {
    codegen_info.output_fifos[i]->push(args[i]);
  }
#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("fire(): freeing %#016lx\n", (size_t)args.data());
#endif
  free(args.data());
}

void VirtualFIFO_Node::fireAlloc(i64 seq) {
  VirtualFIFO_Edge *alloc_fifo = codegen_info.output_fifos.front();

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor(
      "fireAlloc(): fireAlloc %ld of %s\n", seq, codegen_info.name);
#endif

  if (seq == 0) {
    i64 virtual_offset = 0;
    i64 block_size = alloc_fifo->static_info.block_size_with_delays;
    i64 total_delays = alloc_fifo->static_info.delay_offset +
                       alloc_fifo->static_info.delay_size;
    auto chunk = VirtualFIFO_Chunk::allocate(block_size, virtual_offset);

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fireAlloc(): alloc %#016lx of size %ld\n",
                          (size_t)chunk.allocated,
                          block_size);
#endif

    if (total_delays > 0) {
      auto delays = chunk.take_front(total_delays);
      alloc_fifo->propagate_delays(delays);
      virtual_offset += total_delays;
    }
    alloc_fifo->push(chunk);
  } else {
    i64 virtual_offset =
        ((seq - 1) * alloc_fifo->static_info.block_size_no_delays +
         alloc_fifo->static_info.block_size_with_delays);
    i64 block_size = alloc_fifo->static_info.block_size_no_delays;
    auto chunk = VirtualFIFO_Chunk::allocate(block_size, virtual_offset);
    alloc_fifo->push(chunk);

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fireAlloc(): alloc %#016lx of size %ld\n",
                          (size_t)chunk.allocated,
                          block_size);
#endif
  }
}

// Uses the alloc node's semaphore to schedule the allocation.
void VirtualFIFO_Node::ensureAlloc(i64 firing) {

  // Placeholder: figure out a way to keep which blocks were already allocated
  // without an infinitely growing map.
  // All first allocs run on init.
  if (firing == 0)
    return;

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor(
      "ensureAlloc(): firing %ld of %s (expected dependents = %ld)\n",
      firing,
      codegen_info.name,
      static_info.total_iter_firings);

#endif

  bool may_alloc = false;

  VirtualFIFO_AllocSemaphore::FirstArgs f{&may_alloc};
  VirtualFIFO_AllocSemaphore::EveryTimeArgs e{};
  VirtualFIFO_AllocSemaphore::LastArgs l{};

  runtime_info.sema_variant.alloc->semaphore.arrive(
      firing, 1, static_info.total_iter_firings, f, e, l);

  if (may_alloc) {
    fireAlloc(firing);
  }
}

void VirtualFIFO_Node::init() {

  if (static_info.isAlloc()) {
    runtime_info.sema_variant.alloc = new VirtualFIFO_AllocSemaphore{};
  } else {
    runtime_info.sema_variant.normal = new VirtualFIFO_NormalSemaphore{};
  }
  // todo: free this later
};
