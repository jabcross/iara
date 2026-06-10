#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include "IaraRuntime/virtual-fifo/SDFSemaphores.h"
#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/common/WorkStealingBackend.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>

i64 VirtualFIFO_Node::trueInputBytes() const {
  i64 sum = 0;
  for (iara::int_edge i = 0; i < getNumInputs(); i++) {
    auto *e = iara::runtime::virtualfifo::getEdge(getInputEdge(i));
    if (!iara::runtime::virtualfifo::getProducer(e)->runtime_info.isAlloc())
      sum += e->runtime_info.cons_rate;
  }
  return sum;
}

#ifdef IARA_DATA_TRIGGERED_ALLOC
  #include <mutex>

// One physical block can back several firings (a buffer with cons_alpha > 1 is
// gathered by a downstream consumer and must stay contiguous), so blocks are
// allocated once and each firing receives a *view* into the shared allocation —
// exactly the take_front() slicing the alloc node does in the default mode. The
// dealloc node frees the base pointer once, after all slices have flowed
// through, via its own semaphore.
namespace {
struct BlockEntry {
  i8 *base;
  i64 base_voff;
};
gtl::parallel_flat_hash_map<u64, BlockEntry, gtl::priv::hash_default_hash<u64>,
                            gtl::priv::hash_default_eq<u64>,
                            std::allocator<std::pair<const u64, BlockEntry>>, 4,
                            std::mutex>
    g_block_cache;
} // namespace

// Return the slice [begin, end) of the alloc-fed block that this node's firing
// `seq` reads/writes, as a view into the block's single shared allocation.
static VirtualFIFO_Chunk makeAllocChunkForFiring(VirtualFIFO_Edge *e, i64 seq) {
  auto [begin, end] = e->firingOfConsToVirtualOffsetRange(seq);
  i64 block = e->getSingleBlockNumberFromVirtualOffset(begin);

  i64 base_voff, block_size;
  if (block == 0) {
    base_voff = 0;
    block_size = e->runtime_info.block_size_with_delays;
  } else {
    base_voff = e->runtime_info.block_size_with_delays +
                (block - 1) * e->runtime_info.block_size_no_delays;
    block_size = e->runtime_info.block_size_no_delays;
  }

  u64 key = ((u64)iara::runtime::virtualfifo::getEdgeIndex(e) << 32) | (u64)block;
  i8 *base = nullptr;
  bool created = false;
  g_block_cache.lazy_emplace_l(
      key, [&](auto &kv) { base = kv.second.base; },
      [&](auto ctor) {
        base = (i8 *)malloc(block_size);
        created = true;
        ctor(key, BlockEntry{base, base_voff});
      });

  if (created) {
    // A buffer's first block may carry delay tokens inline at its front (a
    // non-feedback initial delay); seed them once on allocation.
    i64 total_delays = e->runtime_info.delay_offset + e->runtime_info.delay_size;
    if (block == 0 && total_delays > 0) {
      VirtualFIFO_Chunk full{
          .allocated = base, .virtual_offset = 0, .data = base,
          .data_size = block_size};
      auto delays = full.take_front(total_delays);
      e->propagate_delays(delays);
    }
  }

  VirtualFIFO_Chunk view;
  view.allocated = base;
  view.virtual_offset = begin;
  view.data = base + (begin - base_voff);
  view.data_size = end - begin;
  return view;
}

void VirtualFIFO_Node::seedFeedbackDelays() {
  if (!runtime_info.isAlloc())
    return;
  auto *e = iara::runtime::virtualfifo::getEdge(getOutputEdge(0));
  i64 total_delays = e->runtime_info.delay_offset + e->runtime_info.delay_size;
  if (total_delays <= 0)
    return;
  // Block 0 of a feedback buffer is the delay/seed region (the producer writes
  // blocks 1+). Allocate it and push the initial delay tokens to the feedback
  // consumer now, mirroring fireAlloc(0), so the first firing of the loop body
  // has its iterPrev input before the data-driven cascade starts.
  i64 block_size = e->runtime_info.block_size_with_delays;
  auto chunk = VirtualFIFO_Chunk::allocate(block_size, 0);
  auto delays = chunk.take_front(total_delays);
  e->propagate_delays(delays);
  // If the block carries data beyond the delays, that is the producer's block-0
  // data; cache the base so its fire() reuses this allocation.
  if (chunk.data_size > 0) {
    u64 key = ((u64)iara::runtime::virtualfifo::getEdgeIndex(e) << 32) | 0u;
    i8 *base = chunk.allocated;
    g_block_cache.lazy_emplace_l(
        key, [](auto &) {},
        [&](auto ctor) { ctor(key, BlockEntry{base, 0}); });
  }
}
#endif // IARA_DATA_TRIGGERED_ALLOC

iara::int_edge VirtualFIFO_Node::getNumOutputs() const {
  if (runtime_info.isAlloc())
    return 1;
  if (runtime_info.isDealloc())
    return 0;
  return runtime_info.num_args;
}

iara::int_edge VirtualFIFO_Node::getOutputEdge(iara::int_edge idx) const {
  if (runtime_info.isAlloc())
    return codegen_info.input_fifos.inline_inputs[0];
  auto *in_e = iara::runtime::virtualfifo::getEdge(getInputEdge(idx));
  auto *out_e = iara::runtime::virtualfifo::getNextInChain(in_e);
  return iara::runtime::virtualfifo::getEdgeIndex(out_e);
}

iara::int_edge VirtualFIFO_Node::getNumInputs() const {
  return runtime_info.num_args;
}

iara::int_edge VirtualFIFO_Node::getInputEdge(iara::int_edge idx) const {
  return (runtime_info.flags & IARA_NODE_INPUTS_INLINE)
      ? codegen_info.input_fifos.inline_inputs[idx]
      : iara_runtime_node_input_fifos_flat[
            codegen_info.input_fifos.indirect.start + idx];
}

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
  debugPrintThreadColor("Consume %ld of node[%lu] kid=%u [%ld] (chunk %ld:%ld) arg_bytes=%ld\n",
                        seq, (size_t)this, (unsigned)codegen_info.kernel_id, arg_idx,
                        chunk.virtual_offset, chunk.virtual_offset + chunk.data_size,
                        runtime_info.arg_bytes);
#endif

#ifdef IARA_DATA_TRIGGERED_ALLOC
  // Fire once the TRUE inputs are complete; the node's own output buffers
  // (alloc-fed inputs) are produced inline by fire(). No priming token.
  i64 arrive_count = trueInputBytes();
#else
  i64 arrive_count = runtime_info.arg_bytes
      + ((runtime_info.flags & IARA_NODE_NEEDS_PRIMING) ? 1 : 0);
#endif
  runtime_info.sema_variant.normal->semaphore.arrive(
      seq, chunk.data_size, arrive_count, f, e, l);

  if (may_fire)
    fire(seq, args);
}

void VirtualFIFO_Node::dealloc(i64 current_buffer_size,
                               i64 first_buffer_size,
                               i64 next_buffer_sizes,
                               VirtualFIFO_Chunk chunk) {
  i64 seq = 0;
  i64 off = chunk.virtual_offset;
  if (chunk.virtual_offset >= first_buffer_size) {
    auto pair = lldiv(chunk.virtual_offset - first_buffer_size, next_buffer_sizes);
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

  if (may_fire)
    fire(seq, args);
}

void VirtualFIFO_Node::prime(i64 seq) {

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("prime(): Priming %ld of node[%lu] kid=%u (total firings %u)\n",
                        seq, (size_t)this, (unsigned)codegen_info.kernel_id,
                        runtime_info.total_iter_firings);
#endif

  for (iara::int_edge i = 0; i < getNumInputs(); i++) {
    auto *fifo = iara::runtime::virtualfifo::getEdge(getInputEdge(i));
    // Only the first kernel of an inout chain — the node the alloc directly
    // feeds — should trigger allocation of a block. Every later kernel in the
    // chain receives the same buffer via push() and cannot run before it
    // exists, so it must not ping the chain-head alloc. Otherwise a downstream
    // consumer re-arrives at the alloc semaphore; on the trivial
    // (single-dependent) path that re-fires the alloc, double-allocating the
    // block — corrupting a kernel's arg span (BarrierTranspose SIGSEGV) and
    // bloating runtime memory ~5x across the whole graph.
    if (!iara::runtime::virtualfifo::getProducer(fifo)->runtime_info.isAlloc())
      continue;
    auto [b, e_off] = fifo->firingOfConsToVirtualOffsetRange(seq);
    auto block = fifo->getSingleBlockNumberFromVirtualOffset(b);
    iara::runtime::virtualfifo::getAllocNode(fifo)->ensureAlloc(block);
  }

  auto f = VirtualFIFO_NormalSemaphore::FirstArgs{this};
  auto e = VirtualFIFO_NormalSemaphore::EveryTimeArgs{
      .data = VirtualFIFO_Chunk::make_empty(), .arg_idx = -1, .first_of_firing = 0};

  bool may_fire = false;
  std::span<VirtualFIFO_Chunk> args;
  auto l = VirtualFIFO_NormalSemaphore::LastArgs{&may_fire, &args};

  i64 arrive_count = runtime_info.arg_bytes
      + ((runtime_info.flags & IARA_NODE_NEEDS_PRIMING) ? 1 : 0);
  runtime_info.sema_variant.normal->semaphore.arrive(
      seq, 1, arrive_count, f, e, l);

  if (may_fire)
    fire(seq, args);
}

void VirtualFIFO_Node::fire(i64 seq, std::span<VirtualFIFO_Chunk> args) {
#ifdef IARA_DATA_TRIGGERED_ALLOC
  // Self-timed feedback can make a firing ready before its iteration is
  // released. Only fire seq < (released iterations) * (firings per iteration).
  i64 limit = iara_data_alloc_run_iter * (i64)runtime_info.total_iter_firings;
  if (seq >= limit) {
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): HOLD %ld of node[%lu] kid=%u (limit %ld)\n",
                          seq, (size_t)this, (unsigned)codegen_info.kernel_id, limit);
#endif
    free(args.data());
    return;
  }
  auto _this = this;
  iara_submit_task([_this, args, seq]() {
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): Firing %ld of node[%lu] kid=%u\n", seq,
                          (size_t)_this, (unsigned)_this->codegen_info.kernel_id);
#endif
    assert((i64)args.size() == (i64)_this->runtime_info.num_args);

    // Produce this node's own output buffers (alloc-fed inputs) inline — the
    // alloc that directly feeds this node fires exactly here, once per firing.
    for (iara::int_edge i = 0; i < _this->getNumInputs(); i++) {
      auto *e = iara::runtime::virtualfifo::getEdge(_this->getInputEdge(i));
      if (iara::runtime::virtualfifo::getProducer(e)->runtime_info.isAlloc())
        args[i] = makeAllocChunkForFiring(e, seq);
    }

    iara::runtime::virtualfifo::fireKernel(_this, seq, args);

    for (iara::int_edge idx = 0; idx < _this->getNumOutputs(); idx++) {
      auto *out = iara::runtime::virtualfifo::getEdge(_this->getOutputEdge(idx));
      // The kernel writes this node's full per-firing output (prod_rate bytes)
      // into a contiguous buffer. The arg chunk's data_size, however, reflects
      // only the first input slice when the input was gathered over several
      // producer firings, so push prod_rate bytes from the buffer start.
      auto out_chunk = args[idx];
      out_chunk.data_size = out->runtime_info.prod_rate;
      out->push(out_chunk);
    }

    free(args.data());
  });
  return;
#else
  auto _this = this;
  iara_submit_task([_this, args, seq]() {
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): Firing %ld of node[%lu]\n", seq, (size_t)_this);
#endif
    assert((i64)args.size() == (i64)_this->runtime_info.num_args);

    iara::runtime::virtualfifo::fireKernel(_this, seq, args);

    for (iara::int_edge idx = 0; idx < _this->getNumOutputs(); idx++) {
      auto *out = iara::runtime::virtualfifo::getEdge(_this->getOutputEdge(idx));
      out->push(args[idx]);
    }

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): freeing %#016lx\n", (size_t)args.data());
#endif
    free(args.data());
  });
#endif // IARA_DATA_TRIGGERED_ALLOC
}

void VirtualFIFO_Node::fireAlloc(i64 seq) {
  auto *alloc_fifo = iara::runtime::virtualfifo::getEdge(getOutputEdge(0));

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor("fireAlloc(): fireAlloc %ld of node[%lu]\n", seq, (size_t)this);
#endif

  if (seq == 0) {
    i64 virtual_offset = 0;
    i64 block_size  = alloc_fifo->runtime_info.block_size_with_delays;
    i64 total_delays = alloc_fifo->runtime_info.delay_offset
                     + alloc_fifo->runtime_info.delay_size;
    auto chunk = VirtualFIFO_Chunk::allocate(block_size, virtual_offset);

    if (total_delays > 0) {
      auto delays = chunk.take_front(total_delays);
      alloc_fifo->propagate_delays(delays);
      virtual_offset += total_delays;
    }
    alloc_fifo->push(chunk);
  } else {
    i64 virtual_offset = ((seq - 1) * alloc_fifo->runtime_info.block_size_no_delays
                          + alloc_fifo->runtime_info.block_size_with_delays);
    i64 block_size = alloc_fifo->runtime_info.block_size_no_delays;
    auto chunk = VirtualFIFO_Chunk::allocate(block_size, virtual_offset);
    alloc_fifo->push(chunk);
  }
}

void VirtualFIFO_Node::ensureAlloc(i64 firing) {
  if (firing == 0)
    return;

#ifdef IARA_DEBUGPRINT
  debugPrintThreadColor(
      "ensureAlloc(): firing %ld of node[%lu] (expected dependents = %u)\n",
      firing, (size_t)this, runtime_info.total_iter_firings);
#endif

  bool may_alloc = false;
  VirtualFIFO_AllocSemaphore::FirstArgs f{&may_alloc};
  VirtualFIFO_AllocSemaphore::EveryTimeArgs e{};
  VirtualFIFO_AllocSemaphore::LastArgs l{};

  runtime_info.sema_variant.alloc->semaphore.arrive(
      firing, 1, runtime_info.total_iter_firings, f, e, l);

  auto _this = this;
  if (may_alloc)
    iara_submit_task([_this, firing]() { _this->fireAlloc(firing); });
}

void VirtualFIFO_Node::init() {
  if (runtime_info.isAlloc()) {
    runtime_info.sema_variant.alloc = new VirtualFIFO_AllocSemaphore{};
  } else {
    runtime_info.sema_variant.normal = new VirtualFIFO_NormalSemaphore{};
  }
}
