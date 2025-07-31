#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

bool is_first_chunk(VirtualFIFO_Edge &fifo,
                    VirtualFIFO_Chunk &,
                    i64 virtual_offset) {
  return virtual_offset < fifo.static_info.block_size_with_delays;
}

// Reads some data and partitions it into the pieces that will be consumed in
// the different firings of the consumer actor.
void VirtualFIFO_Edge::push(VirtualFIFO_Chunk chunk) {
  VirtualFIFO_Chunk remaining_data = std::move(chunk);
  auto cons_rate = static_info.cons_rate;
  // dealloc
  if (cons_rate < 0) {
    ((VirtualFIFO_Node *)codegen_info.consumer)
        ->dealloc(((chunk.virtual_offset < static_info.block_size_with_delays)
                       ? static_info.block_size_with_delays
                       : static_info.block_size_no_delays),
                  static_info.block_size_with_delays,
                  static_info.block_size_no_delays,
                  chunk);
    return;
  }
  while (remaining_data.data_size > 0) {
    auto [seq, off, size] =
        static_info.getConsumerSlice(remaining_data.virtual_offset);
    auto front =
        remaining_data.take_front(std::min(size, remaining_data.data_size));
    codegen_info.consumer->consume(seq, front, static_info.cons_arg_idx, off);
  }
  assert(remaining_data.data_size == 0);
  remaining_data.release();
}

// Pushes delay data into FIFOs.
void VirtualFIFO_Edge::propagate_delays(VirtualFIFO_Chunk chunk) {
  if (codegen_info.delay_data.extents > 0) {
    assert((size_t)chunk.data_size >= codegen_info.delay_data.extents);
    auto this_delay = chunk.take_back(codegen_info.delay_data.extents);
    memcpy(this_delay.data,
           codegen_info.delay_data.ptr,
           codegen_info.delay_data.extents);
    push(this_delay);
  }
  if (chunk.data_size == 0 || codegen_info.next_in_chain == nullptr)
    return;
  codegen_info.next_in_chain->propagate_delays(chunk);
}
