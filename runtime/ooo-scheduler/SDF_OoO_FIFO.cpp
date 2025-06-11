#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include <cstring>
#include <utility>

bool is_first_chunk(SDF_OoO_FIFO &fifo, Chunk &, i64 ooo_offset) {
  return ooo_offset < fifo.info.first_chunk_size;
}

PairOf<i64> SDF_OoO_FIFO::get_consumer_seq(i64 ooo_offset) {
  assert(info.cons_rate != -1);
  if (ooo_offset < info.first_chunk_size) {
    auto rv = lldiv(ooo_offset - info.delay_offset, info.cons_rate);
    return {rv.quot, rv.rem};
  }
  ooo_offset -= info.first_chunk_size;
  auto rv = lldiv(ooo_offset, info.cons_rate);
  return {rv.quot, rv.rem};
}

// Reads some data and partitions it into the pieces that will be consumed in
// the different firings of the consumer actor.
void SDF_OoO_FIFO::push(Chunk chunk) {
  Chunk remaining_data = std::move(chunk);
  auto cons_rate = info.cons_rate;
  // dealloc
  if (cons_rate < 0) {
    ((SDF_OoO_Node *)consumer)
        ->dealloc(((chunk.ooo_offset < info.first_chunk_size)
                       ? info.first_chunk_size
                       : info.next_chunk_sizes),
                  info.first_chunk_size,
                  info.next_chunk_sizes,
                  chunk);
    return;
  }
  while (remaining_data.data_size >= (size_t)info.cons_rate) {
    auto front = remaining_data.take_front(info.cons_rate);
    auto [seq, off] = get_consumer_seq(front.ooo_offset);
    consumer->consume(seq, front, info.cons_arg_idx, off);
  }
  if (!remaining_data.is_released()) {
    auto [seq, off] = get_consumer_seq(remaining_data.ooo_offset);
    consumer->consume(seq, remaining_data, info.cons_arg_idx, off);
  }
}
// Pushes delay data into FIFOs.
void SDF_OoO_FIFO::propagate_delays(Chunk chunk) {
  if (delay_data.extents > 0) {
    assert(chunk.data_size >= delay_data.extents);
    auto this_delay = chunk.take_back(delay_data.extents);
    memcpy(this_delay.data, delay_data.ptr, delay_data.extents);
    push(this_delay);
  }
  if (chunk.data_size == 0 || next_in_chain == nullptr)
    return;
  next_in_chain->propagate_delays(chunk);
}
