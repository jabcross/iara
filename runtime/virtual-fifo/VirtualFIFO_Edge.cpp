#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstring>
#include <utility>

#ifdef IARA_DEBUGPRINT
  #include "IaraRuntime/util/DebugPrint.h"
  #include <mutex>
extern std::mutex debug_mutex;
#endif

bool is_first_chunk(VirtualFIFO_Edge &fifo, VirtualFIFO_Chunk &, i64 virtual_offset) {
  return virtual_offset < fifo.runtime_info.block_size_with_delays;
}

void VirtualFIFO_Edge::push(VirtualFIFO_Chunk chunk) {
  VirtualFIFO_Chunk remaining_data = std::move(chunk);
  auto cons_rate = runtime_info.cons_rate;
  // dealloc edge: cons_rate < 0
  if (cons_rate < 0) {
    iara::runtime::virtualfifo::getConsumer(this)
        ->dealloc(((chunk.virtual_offset < runtime_info.block_size_with_delays)
                       ? runtime_info.block_size_with_delays
                       : runtime_info.block_size_no_delays),
                  runtime_info.block_size_with_delays,
                  runtime_info.block_size_no_delays,
                  chunk);
    return;
  }
  while (remaining_data.data_size > 0) {
    auto [seq, off, size] = runtime_info.getConsumerSlice(remaining_data.virtual_offset);
    auto front = remaining_data.take_front(std::min(size, remaining_data.data_size));
#ifdef IARA_DEBUGPRINT
    debugPrintThreadColor("push(): edge[%lu] -> node[%lu][%ld] (seq %ld, chunk %ld:%ld)\n",
                          iara::runtime::virtualfifo::getEdgeIndex(this),
                          (size_t)iara::runtime::virtualfifo::getConsumer(this),
                          runtime_info.cons_arg_idx, seq,
                          front.virtual_offset, front.virtual_offset + front.data_size);
#endif
    iara::runtime::virtualfifo::getConsumer(this)->consume(
        seq, front, runtime_info.cons_arg_idx, off);
  }
  assert(remaining_data.data_size == 0);
  remaining_data.release();
}

void VirtualFIFO_Edge::propagate_delays(VirtualFIFO_Chunk chunk) {
  if (runtime_info.delay_size > 0) {
    auto this_delay = chunk.take_back(runtime_info.delay_size);
#ifdef IARA_DELAYS_ZERO_INIT
    // Codegen confirmed delay values are all-zero; memset is cheaper.
    memset(this_delay.data, 0, runtime_info.delay_size);
#else
    const unsigned char *delay_data =
        iara_runtime_edge_delays_flat + codegen_info.delay_start;
    assert((size_t)chunk.data_size >= (size_t)runtime_info.delay_size);
    memcpy(this_delay.data, delay_data, runtime_info.delay_size);
#endif
    push(this_delay);
  }
  auto *next = iara::runtime::virtualfifo::getNextInChain(this);
  if (chunk.data_size == 0 || next == nullptr)
    return;
  next->propagate_delays(chunk);
}
