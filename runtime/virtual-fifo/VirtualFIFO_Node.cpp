#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include "IaraRuntime/virtual-fifo/SDFSemaphores.h"
#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/MockAllocator.h"
#include "IaraRuntime/common/WorkStealingBackend.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <gtl/phmap.hpp>

i64 VirtualFIFO_Node::trueInputBytes() const {
  // Walk the FULL input-fifo slice (data inputs + any appended logic inputs),
  // summing cons_rate for everything not fed by an alloc node (a node produces
  // its own alloc-fed buffers inline in fire(), so those don't gate). This
  // reconstructs the old logic_in_bytes field: logic inputs (a join's reader
  // tokens, cons_arg_idx == -1) have a non-alloc producer and cons_rate == their
  // per-firing token count, so they fall out of the same loop -- in i64, no u8
  // overflow. It also keeps a logic-only node (a join) from looking like a
  // source. getNumInputs() (num_args) stays the kernel-arg count; getNumInputEdges
  // covers the logic tail (the edges are contiguous, so no extra cache misses).
  i64 sum = 0;
  for (iara::int_edge i = 0; i < getNumInputEdges(); i++) {
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
#ifdef IARA_MOCK_ALLOC
        base = (i8 *)iara_mock_alloc(block_size);
#else
        base = (i8 *)malloc(block_size);
#endif
        created = true;
        ctor(key, BlockEntry{base, base_voff});
      });

  if (created) {
    // A buffer's first block may carry delay tokens inline at its front (a
    // non-feedback initial delay); seed them once on allocation.
    i64 total_delays = e->runtime_info.delay_offset + e->runtime_info.delay_size;
    if (block == 0 && total_delays > 0) {
      VirtualFIFO_Chunk full{
          .allocated = base, .data = base, .data_size = block_size};
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

// Seed the front [0,D) of each DELAYED borrow output's virtual stream with the
// edge's initial delay tokens, so a delayed-borrow reader's firing 0 gets the
// initial instance before the producer's aliases start (fireBroadcast shifts
// those past [0,D)). Mirrors seedFeedbackDelays for the borrow path. The tokens
// come from iara_runtime_edge_delays_flat via propagate_delays -- delays may
// carry real init values, not just zeros, so this memcpy (not a memset) is
// required. Only broadcasts have borrow outputs; delay_size == 0 skips plain
// (non-delayed) borrows.
void VirtualFIFO_Node::seedBorrowDelays() {
  if (!(runtime_info.flags & IARA_NODE_IS_BROADCAST))
    return;
  for (iara::int_edge k = 0; k < getNumLogicOutputs(); k++) {
    auto *be = iara::runtime::virtualfifo::getEdge(getLogicOutputEdge(k));
    i64 D = be->runtime_info.delay_size;
    if (D <= 0)
      continue;
    // ponytail: the seed buffer (D bytes) is a one-time owned allocation with no
    // dealloc node, so it outlives firing 0 (a small one-shot leak, << the GBs of
    // copies eliminated). Give it a real dealloc path if the leak ever matters.
    auto chunk = VirtualFIFO_Chunk::allocate(D, 0);
    auto delays = chunk.take_front(D);
    be->propagate_delays(delays);
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

void VirtualFIFO_Node::consumeLogic(i64 seq, i64 tokens) {
  // A logic (control-only) arrival gates firing without occupying a kernel-arg
  // slot: an empty chunk (skipped by every_time_func's store) plus an explicit
  // token amount — the exact decoupling prime() uses. The threshold matches
  // consume()'s so all arrivals for a firing agree on it.
  auto f = VirtualFIFO_NormalSemaphore::FirstArgs{this};
  auto e = VirtualFIFO_NormalSemaphore::EveryTimeArgs{
      .data = VirtualFIFO_Chunk::make_empty(), .arg_idx = -1,
      .first_of_firing = 0};

  bool may_fire = false;
  std::span<VirtualFIFO_Chunk> args;
  auto l = VirtualFIFO_NormalSemaphore::LastArgs{&may_fire, &args};

#ifdef IARA_DATA_TRIGGERED_ALLOC
  // trueInputBytes() now includes this node's logic-input tokens, so the
  // threshold agrees with consume()'s and logic edges gate correctly.
  i64 arrive_count = trueInputBytes();
#else
  i64 arrive_count = runtime_info.arg_bytes
      + ((runtime_info.flags & IARA_NODE_NEEDS_PRIMING) ? 1 : 0);
#endif
  runtime_info.sema_variant.normal->semaphore.arrive(
      seq, tokens, arrive_count, f, e, l);

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

void VirtualFIFO_Node::fireBroadcast(i64 seq, std::span<VirtualFIFO_Chunk> args) {
#ifdef IARA_DATA_TRIGGERED_ALLOC
  i64 limit = iara_data_alloc_run_iter * (i64)runtime_info.total_iter_firings;
  if (seq >= limit) {
#ifdef IARA_SEMAPHORE_ATOMIC_RING
    runtime_info.sema_variant.normal->semaphore.release(seq);
#else
    free(args.data());
#endif
    return;
  }
#endif
  auto _this = this;
  iara_submit_task([_this, args, seq]() {
    using namespace iara::runtime::virtualfifo;
    auto input = args[0];
    // Owned buffer -> the join (chain output 0), read-write: allocated stays the
    // real base so the join's dealloc frees it once, after all readers finish.
    auto *owner = getEdge(_this->getOutputEdge(0));
    auto owned = input;
    owned.data_size = owner->runtime_info.prod_rate;
    owner->push(owned);
    // Read-only borrows -> every reader (the logic_out range for a broadcast).
    // The reader fires K = prod/cons times per buffer; deliver one borrow per
    // firing, each aliasing the L-byte physical buffer (allocated=null: no
    // borrower path frees it). The logical offset j*cons is wrapped mod L in the
    // reader's wrapper (toroidal layout map), so a replication reader (prod > L)
    // re-reads the same physical bytes zero-copy.
    for (iara::int_edge k = 0; k < _this->getNumLogicOutputs(); k++) {
      auto *be = getEdge(_this->getLogicOutputEdge(k));
      i64 cons = be->runtime_info.cons_rate;
      i64 prod = be->runtime_info.prod_rate;
      i64 K = (cons > 0) ? (prod / cons) : 1;
      for (i64 j = 0; j < K; j++) {
        auto borrow = input;
        borrow.allocated = nullptr;
        borrow.data_size = cons;
        borrow.offset = j * cons; // logical, within THIS firing's buffer; wrapped mod L
        // FIFO routing is GLOBAL across broadcast firings: this firing (seq)
        // produces `prod` bytes on the edge, so its K sub-pushes land at
        // seq*prod + j*cons. Using only j*cons collides seq>0 firings onto
        // seq 0's already-consumed slots -> readers past the first buffer starve
        // (deadlock in multi-firing broadcasts like SIFT's octave loop).
        // A DELAYED borrow (delay_size > 0) shifts the whole stream past the
        // seed region [0,D) so this firing's instance lands at the reader's
        // NEXT slot -> the reader aliases the PREVIOUS instance (the delay).
        // seedBorrowDelays fills [0,D). delay_size == 0 for a plain borrow.
        borrow.virtual_offset =
            be->runtime_info.delay_size + seq * prod + j * cons;
        be->push(borrow);
      }
    }
#ifdef IARA_SEMAPHORE_ATOMIC_RING
    _this->runtime_info.sema_variant.normal->semaphore.release(seq);
#else
    free(args.data());
#endif
  });
}

void VirtualFIFO_Node::fire(i64 seq, std::span<VirtualFIFO_Chunk> args) {
  if (runtime_info.flags & IARA_NODE_IS_BROADCAST) {
    fireBroadcast(seq, args);
    return;
  }
#ifdef IARA_DATA_TRIGGERED_ALLOC
  // Self-timed feedback can make a firing ready before its iteration is
  // released. Only fire seq < (released iterations) * (firings per iteration).
  i64 limit = iara_data_alloc_run_iter * (i64)runtime_info.total_iter_firings;
  if (seq >= limit) {
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): HOLD %ld of node[%lu] kid=%u (limit %ld)\n",
                          seq, (size_t)this, (unsigned)codegen_info.kernel_id, limit);
#endif
#ifdef IARA_SEMAPHORE_ATOMIC_RING
    runtime_info.sema_variant.normal->semaphore.release(seq);
#else
    free(args.data());
#endif
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
      auto out_idx = _this->getOutputEdge(idx);
      // A borrow (read-only alias) input has no chain-successor output; its
      // getOutputEdge(idx) = input+1 lands on an unrelated edge (or past the
      // end). Skip any output this node does not actually produce.
      if (out_idx >= (iara::int_edge)iara::runtime::virtualfifo::getNumEdges())
        continue;
      auto *out = iara::runtime::virtualfifo::getEdge(out_idx);
      if (iara::runtime::virtualfifo::getProducer(out) != _this)
        continue;
      // The kernel writes this node's full per-firing output (prod_rate bytes)
      // into a contiguous buffer. The arg chunk's data_size, however, reflects
      // only the first input slice when the input was gathered over several
      // producer firings, so push prod_rate bytes from the buffer start.
      auto out_chunk = args[idx];
      out_chunk.data_size = out->runtime_info.prod_rate;
      out->push(out_chunk);
    }

    // Deliver control tokens on this node's logic outputs. A multi-rate logic
    // edge (cons_rate = this reader's firings per join firing) maps this
    // producer firing to the consumer's (join's) firing and delivers prod_rate
    // (=1) token, so a reader firing R times gates exactly one join firing (see
    // convertBroadcastToBorrow). A 1:1 logic edge is unchanged (cons_seq = seq).
    for (iara::int_edge k = 0; k < _this->getNumLogicOutputs(); k++) {
      auto *le =
          iara::runtime::virtualfifo::getEdge(_this->getLogicOutputEdge(k));
      i64 prod = le->runtime_info.prod_rate;
      i64 cons = le->runtime_info.cons_rate;
      i64 cons_seq = (cons > 0) ? (seq * prod / cons) : seq;
      // Delayed-borrow reader: this firing read the PREVIOUS buffer instance
      // (B_{k-shift}), so its token frees join firing (cons_seq - shift) — the
      // join that owns the buffer it read. shift == 0 for normal logic edges.
      // The first `shift` firings read the (leaked) seed buffers, owned by no
      // join firing (negative) — drop those tokens.
      cons_seq -= le->runtime_info.delay_offset;
      if (cons_seq < 0)
        continue;
      iara::runtime::virtualfifo::getConsumer(le)->consumeLogic(cons_seq, prod);
    }

#ifdef IARA_SEMAPHORE_ATOMIC_RING
    _this->runtime_info.sema_variant.normal->semaphore.release(seq);
#else
    free(args.data());
#endif
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
      auto out_idx = _this->getOutputEdge(idx);
      // Skip a borrow input's phantom output (see the data-triggered path).
      if (out_idx >= (iara::int_edge)iara::runtime::virtualfifo::getNumEdges())
        continue;
      auto *out = iara::runtime::virtualfifo::getEdge(out_idx);
      if (iara::runtime::virtualfifo::getProducer(out) != _this)
        continue;
      out->push(args[idx]);
    }

    // Deliver control tokens on this node's logic outputs. A multi-rate logic
    // edge (cons_rate = this reader's firings per join firing) maps this
    // producer firing to the consumer's (join's) firing and delivers prod_rate
    // (=1) token, so a reader firing R times gates exactly one join firing (see
    // convertBroadcastToBorrow). A 1:1 logic edge is unchanged (cons_seq = seq).
    for (iara::int_edge k = 0; k < _this->getNumLogicOutputs(); k++) {
      auto *le =
          iara::runtime::virtualfifo::getEdge(_this->getLogicOutputEdge(k));
      i64 prod = le->runtime_info.prod_rate;
      i64 cons = le->runtime_info.cons_rate;
      i64 cons_seq = (cons > 0) ? (seq * prod / cons) : seq;
      // Delayed-borrow reader: this firing read the PREVIOUS buffer instance
      // (B_{k-shift}), so its token frees join firing (cons_seq - shift) — the
      // join that owns the buffer it read. shift == 0 for normal logic edges.
      // The first `shift` firings read the (leaked) seed buffers, owned by no
      // join firing (negative) — drop those tokens.
      cons_seq -= le->runtime_info.delay_offset;
      if (cons_seq < 0)
        continue;
      iara::runtime::virtualfifo::getConsumer(le)->consumeLogic(cons_seq, prod);
    }

#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("fire(): freeing %#016lx\n", (size_t)args.data());
#endif
#ifdef IARA_SEMAPHORE_ATOMIC_RING
    _this->runtime_info.sema_variant.normal->semaphore.release(seq);
#else
    free(args.data());
#endif
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
    // Alloc semaphore is always the map variant (see SDFSemaphores.h) — no ring
    // to reserve; its total_iter_firings is a dependent count, not a firing
    // count.
    runtime_info.sema_variant.alloc = new VirtualFIFO_AllocSemaphore{};
  } else {
    runtime_info.sema_variant.normal = new VirtualFIFO_NormalSemaphore{};
#ifdef IARA_SEMAPHORE_ATOMIC_RING
    runtime_info.sema_variant.normal->semaphore.reserve(
        runtime_info.total_iter_firings);
#endif
  }
}
