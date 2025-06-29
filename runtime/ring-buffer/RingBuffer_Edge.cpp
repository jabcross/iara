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

// static std::mutex debug_mutex;

// void printChunkOfInts(Chunk *chunk) {
//   auto size = chunk->data_size / sizeof(int);
//   auto first = (int *)chunk->data;

//   for (int i = 0; i < size; i++) {
//     debugPrintThreadColor("%d ", first[i]);
//   }
//   fprintf(stderr, "\n");
//   fflush(stderr);
// }

// void printChunkOfFloats(Chunk *chunk) {
//   auto size = chunk->data_size / sizeof(int);
//   auto first = (float *)chunk->data;

//   for (int i = 0; i < size; i++) {
//     debugPrintThreadColor("%.0f \e[0m", first[i]);
//   }
//   fprintf(stderr, "\n");
//   fflush(stderr);
// }

// void dumpQueue(RingBuffer_Edge *edge) {
//   auto &queue = edge->queue;
//   queue->m_mutex.lock();

//   debugPrintThreadColor("%s size %lu\n", edge->name, queue->m_size);
//   debugPrintThreadColor("[ ");
//   Chunk c;
//   c.data = queue->m_buffer;
//   c.data_size = queue->m_capacity;
//   printChunkOfFloats(&c);
//   auto f = queue->m_front - queue->m_buffer;
//   auto b = queue->m_back - queue->m_buffer;
//   f /= 4;
//   b /= 4;
//   fprintf(stderr, "  ");
//   for (int i = 0; i < queue->m_capacity / 4 + 1; i++) {
//     if (i == f) {
//       debugPrintThreadColor("F ");
//     } else
//       fprintf(stderr, "  ");
//   }
//   fprintf(stderr, "\n  ");
//   for (int i = 0; i < queue->m_capacity / 4 + 1; i++) {
//     if (i == b) {
//       debugPrintThreadColor("B ");
//     } else
//       fprintf(stderr, "  ");
//   }
//   fprintf(stderr, "\n");

//   queue->m_mutex.unlock();
// }

void RingBuffer_Edge::init() {
  queue = new MutexRingBuffer(64);
  if (delay_data.extents > 0) {
    push({(i8 *)delay_data.ptr, (i8 *)delay_data.ptr, (i64)delay_data.extents});
  }
  // int firsts[5];
  // Chunk c{(i8 *)firsts, (i8 *)firsts, 4 * 5};
  // pop(c);
  // for (int i = 0; i < 5; i++)
  //   fprintf(stderr, "%d ", firsts[i]);
  // fprintf(stderr, "\n");
  // fflush(stderr);
}

// Push data into fifo. Blocks.
void RingBuffer_Edge::push(Chunk chunk) {
  // debug_mutex.lock();
  // debugPrintThreadColor(
  //     "   Pushing %ld bytes to edge %ld queue %#016lx size %lu:\n   ",
  //     chunk.data_size,
  //     info.id,
  //     (size_t)queue,
  //     queue->m_size);
  // printChunkOfFloats(&chunk);
  // dumpQueue(this);
  // debug_mutex.unlock();
  queue->push(chunk.data, chunk.data_size);
  // debug_mutex.lock();
  // dumpQueue(this);
  // debug_mutex.unlock();
}

// Read data from fifo. Blocks.
void RingBuffer_Edge::pop(Chunk &chunk) {

  // debug_mutex.lock();
  // debugPrintThreadColor(
  //     "   Popping %ld bytes from edge %ld queue %#016lx size %lu\n ",
  //     chunk.data_size,
  //     info.id,
  //     (size_t)queue,
  //     queue->m_size);
  // dumpQueue(this);
  // debug_mutex.unlock();
  auto ptr = chunk.data;
  queue->pop(ptr, chunk.data_size);
  // debug_mutex.lock();
  // printChunkOfFloats(&chunk);
  // dumpQueue(this);
  // debug_mutex.unlock();
}
