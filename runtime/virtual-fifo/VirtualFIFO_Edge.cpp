#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cstring>
#include <utility>

bool is_first_chunk(VirtualFIFO_Edge &fifo, Chunk &, i64 virtual_offset) {
  return virtual_offset < fifo.info.block_size_with_delays;
}

// Reads some data and partitions it into the pieces that will be consumed in
// the different firings of the consumer actor.
void VirtualFIFO_Edge::push(Chunk chunk) {
  Chunk remaining_data = std::move(chunk);
  auto cons_rate = info.cons_rate;
  // dealloc
  if (cons_rate < 0) {
    ((VirtualFIFO_Node *)consumer)
        ->dealloc(((chunk.virtual_offset < info.block_size_with_delays)
                       ? info.block_size_with_delays
                       : info.block_size_no_delays),
                  info.block_size_with_delays,
                  info.block_size_no_delays,
                  chunk);
    return;
  }
  while (remaining_data.data_size > 0) {
    auto [seq, off, size] =
        info.getConsumerSlice(remaining_data.virtual_offset);
    auto front =
        remaining_data.take_front(std::min(size, remaining_data.data_size));
    consumer->consume(seq, front, info.cons_arg_idx, off);
  }
  assert(remaining_data.data_size == 0);
  remaining_data.release();
}

// Pushes delay data into FIFOs.
void VirtualFIFO_Edge::propagate_delays(Chunk chunk) {
  if (delay_data.extents > 0) {
    assert((size_t)chunk.data_size >= delay_data.extents);
    auto this_delay = chunk.take_back(delay_data.extents);
    memcpy(this_delay.data, delay_data.ptr, delay_data.extents);
    push(this_delay);
  }
  if (chunk.data_size == 0 || next_in_chain == nullptr)
    return;
  next_in_chain->propagate_delays(chunk);
}
