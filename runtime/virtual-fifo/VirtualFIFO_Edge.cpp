#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#ifdef IARA_DEBUGPRINT
  #include "IaraRuntime/util/DebugPrint.h"
  #include <mutex>
extern std::mutex debug_mutex;
#endif

bool is_first_chunk(VirtualFIFO_Edge &fifo,
                    VirtualFIFO_Chunk &,
                    i64 virtual_offset) {
  return virtual_offset < fifo.static_info.block_size_with_delays;
}

// Reads some data and partitions it into the pieces that will be consumed by
// the different firings of the consumer actor.
void VirtualFIFO_Edge::push(VirtualFIFO_Chunk chunk) {
  VirtualFIFO_Chunk remaining_data = std::move(chunk);
  auto cons_rate = static_info.cons_rate;
  // dealloc
  if (cons_rate < 0) {
    iara::runtime::virtualfifo::getConsumer(this)
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
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("push(): %s -> %s[%ld] (seq %ld, chunk %ld:%ld, cons_rate %ld, slice %ld)\n",
                          codegen_info.name, iara::runtime::virtualfifo::getConsumer(this)->codegen_info.name,
                          static_info.cons_arg_idx, seq, front.virtual_offset,
                          front.virtual_offset + front.data_size, static_info.cons_rate, size);
#endif
    iara::runtime::virtualfifo::getConsumer(this)->consume(seq, front, static_info.cons_arg_idx, off);
  }
  assert(remaining_data.data_size == 0);
  remaining_data.release();
}

// Pushes delay data into FIFOs.
void VirtualFIFO_Edge::propagate_delays(VirtualFIFO_Chunk chunk) {
  if (codegen_info.delay_data.size() > 0) {
    assert((size_t)chunk.data_size >= codegen_info.delay_data.size_bytes());
    auto this_delay = chunk.take_back(codegen_info.delay_data.size());
    memcpy(this_delay.data,
           codegen_info.delay_data.data(),
           codegen_info.delay_data.size_bytes());
    push(this_delay);
  }
  auto *next = iara::runtime::virtualfifo::getNextInChain(this);
  if (chunk.data_size == 0 || next == nullptr)
    return;
  next->propagate_delays(chunk);
}
