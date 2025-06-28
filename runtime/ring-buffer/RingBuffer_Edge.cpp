#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/ring-buffer/Chunk.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <cstdio>
#include <cstring>
#include <omp.h>
#include <thread>

void printChunkOfInts(Chunk *chunk) {
  auto size = chunk->data_size / sizeof(int);
  auto first = (int *)chunk->data;

  auto offset = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 7;

  char colorcode[20];

  sprintf(colorcode, "\e[%2lum", 31 + offset);

  for (int i = 0; i < size; i++) {
    fprintf(stderr, "%s%d \e[0m", colorcode, first[i]);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

void RingBuffer_Edge::init() {
  queue = new moodycamel::BlockingConcurrentQueue<char>{};
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
  auto offset = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 7;

  char colorcode[20];

  sprintf(colorcode, "\e[%2lum", 31 + offset);
  fprintf(stderr,
          "%s   Pushing %ld bytes to edge %ld queue %#016lx:\n   ",
          colorcode,
          chunk.data_size,
          info.id,
          (size_t)queue);
  printChunkOfInts(&chunk);
  queue->enqueue_bulk(chunk.data, chunk.data_size);
}

// Read data from fifo. Blocks.
void RingBuffer_Edge::pop(Chunk &chunk) {
  auto offset = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 7;

  char colorcode[20];

  sprintf(colorcode, "\e[%2lum", 31 + offset);
  fprintf(stderr,
          "%s   Popping %ld bytes from edge %ld queue %#016lx\n ",
          colorcode,
          chunk.data_size,
          info.id,
          (size_t)queue);
  fflush(stderr);
  i64 remaining = chunk.data_size;
  auto ptr = chunk.data;

  while (remaining > 0) {
    auto dequeued = queue->wait_dequeue_bulk(ptr, remaining);
    ptr += dequeued;
    remaining -= dequeued;

    fprintf(stderr,
            "%s   Popped %ld bytes from edge %ld, remaining: %ld\n   ",
            colorcode,
            dequeued,
            info.id,
            remaining);
    printChunkOfInts(&chunk);
    fflush(stderr);
  }
  fprintf(stderr,
          "%s   Popped %ld bytes from edge %ld:\n   ",
          colorcode,
          chunk.data_size,
          info.id);
  printChunkOfInts(&chunk);
}
