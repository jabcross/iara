#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include <cassert>
#include <cstdlib>

extern std::span<SDF_OoO_Node> iara_runtime_nodes;
extern std::span<SDF_OoO_FIFO> iara_runtime_edges;

extern "C" void kickstart_alloc(SDF_OoO_Node *alloc) {
  auto &fifo = **alloc->output_fifos.ptr;
  auto remaining_firings = alloc->info.total_firings;
  assert(remaining_firings > 0);
  i64 ooo_offset = 0;
  if (fifo.info.delay_size > 0) {
    ooo_offset += fifo.info.delay_size;
    remaining_firings -= 1;
    fifo.push(Chunk::allocate(fifo.info.first_chunk_size, 0));
  }
  while (remaining_firings > 0) {
    fifo.push(Chunk::allocate(fifo.info.next_chunk_sizes, ooo_offset));
    remaining_firings -= 1;
    ooo_offset += fifo.info.next_chunk_sizes;
  }
}

extern "C" void iara_runtime_alloc(i64 seq, Chunk *chunk) {
  chunk->allocated = (byte *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, Chunk *chunk) {
  free(chunk->allocated);
}
