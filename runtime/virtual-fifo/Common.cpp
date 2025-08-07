#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"

// This implementation is used by both the compiler and runtime.

ConsData VirtualFIFO_Edge_StaticInfo::getConsumerSlice(i64 virtual_offset) {
  assert(cons_rate != -1);
  if (virtual_offset < block_size_with_delays) {
    auto [seq, cons_offset] = lldiv(virtual_offset - delay_offset, cons_rate);
    auto size = cons_rate - cons_offset;
    return {seq, cons_offset, size};
  }
  auto no_delays_offset = virtual_offset - block_size_with_delays;
  auto [seq, cons_offset] = lldiv(no_delays_offset, cons_rate);
  auto size = cons_rate - cons_offset;
  return {seq + cons_alpha, cons_offset, size};
}
