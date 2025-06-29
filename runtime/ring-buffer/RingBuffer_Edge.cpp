#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include "IaraRuntime/ring-buffer/MutexRingBuffer.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include "IaraRuntime/util/DebugPrint.h"
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <cstdio>
#include <cstring>
#include <omp.h>
#include <thread>

void RingBuffer_Edge::init() {
  queue = new MutexRingBuffer(64);
  if (delay_data.extents > 0) {
    push({(i8 *)delay_data.ptr, (i8 *)delay_data.ptr, (i64)delay_data.extents});
  }
}

// Push data into fifo. Blocks.
void RingBuffer_Edge::push(Chunk chunk) {
  queue->push(chunk.data, chunk.data_size, info.id);
}

// Read data from fifo.
bool RingBuffer_Edge::tryPop(Chunk &chunk) {
  auto ptr = chunk.data;
  return queue->tryPop(ptr, chunk.data_size, info.id);
}
