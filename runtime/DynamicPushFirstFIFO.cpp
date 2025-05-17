#include "IaraRuntime/DynamicPushFirstFIFO.h"
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <unordered_map>

struct DynamicPushFirstFifo::Impl {

  struct Block {
    byte *allocated;
    i64 size;
    i64 consumed = 0;
    i64 num_borrows = 0;

    // Allocate the new buffer automatically.
    Block(i64 alloc_size)
        : allocated((byte *)malloc(alloc_size)), size(alloc_size) {}

    // Give a preallocated buffer to the new block.
    Block(i64 preallocated_size, byte *data)
        : allocated(data), size(preallocated_size) {}
  };

  // keeps track of which blocks have been consumed by pop
  i64 block_sequence_number = 0;

  // keeps the ordering of the pop callers
  i64 pop_sequence_number = 0;
  std::condition_variable pop_cv;

  std::mutex mutex;
  std::unordered_map<i64 /* block number */, Block> blocks;
  std::condition_variable block_cv; // Notified when a new block is inserted.

  static Impl *create() { return new Impl(); };

  // creates a block with the given size, data and sequence number.
  // It is the programmer's responsability to guarantee that sequence_number
  // will be different at each call to this function.
  void push(i64 size, i64 sequence_number, byte *data) {
    auto lock = std::unique_lock(mutex);
    auto it = blocks.find(sequence_number);
    assert(it == blocks.end());
    blocks.try_emplace(sequence_number, size, data);
    lock.unlock();
    block_cv.notify_all();
  }

  // Returns a buffer with the front of the queue. If the block matches exactly
  // the expected size size, no allocations/copies need to happen.
  byte *pop(i64 size, i64 sequence_number) {
    auto lock = std::unique_lock(mutex);
    if (sequence_number != pop_sequence_number) {
      pop_cv.wait(lock,
                  [&]() { return sequence_number == pop_sequence_number; });
    }

    byte *rv = nullptr;
    i64 rv_size = 0;

    while (rv_size < size) {
      auto it = blocks.find(block_sequence_number);
      if (it == blocks.end()) {
        block_cv.wait(lock, [&]() {
          it = blocks.find(block_sequence_number);
          return it != blocks.end();
        });
      }
      auto &block = it->second;
      if (rv == nullptr and block.size == size) {
        // No copy needed. Return the block's buffer directly.
        rv = block.allocated;
        rv_size = size;
        blocks.erase(it);
        block_sequence_number++; // Next pop will look for the next block.
        break;
      }

      // We'll need to copy the values into a new buffer.
      rv = (byte *)malloc(size);
      auto remaining_size = size - rv_size;
      auto block_available_data = block.size - block.consumed;
      auto copy_size = std::min(remaining_size, block_available_data);
      std::memcpy(rv + rv_size, block.allocated + block.consumed, copy_size);
      rv_size += copy_size;
      block.consumed += copy_size;

      if (block.consumed == block.size) {
        // We're done with this block.
        blocks.erase(it);
        block_sequence_number++; // Next pop will look for the next block.
      }
    }
    assert(rv_size == size);
    assert(rv != nullptr);
    pop_sequence_number++;
    lock.unlock();
    pop_cv.notify_all();
    return rv;
  }
};

DynamicPushFirstFifo *DynamicPushFirstFifo::create() {
  Impl *pimpl;
  pimpl = Impl::create();
  DynamicPushFirstFifo *fifo = new DynamicPushFirstFifo{};
  fifo->pimpl = pimpl;
  return fifo;
}

void DynamicPushFirstFifo::push(i64 size, i64 sequence_number, byte *data) {
  pimpl->push(size, sequence_number, data);
}

byte *DynamicPushFirstFifo::pop(i64 size, i64 sequence_number) {
  return pimpl->pop(size, sequence_number);
}

// C wrappers

extern "C" void *iara_fifo_runtime_DynamicPushFirstFifo_create() {
  return (void *)DynamicPushFirstFifo::create();
};

extern "C" void iara_fifo_runtime_DynamicPushFirstFifo_push(void *fifo,
                                                            i64 size,
                                                            i64 sequence_number,
                                                            byte *data) {
  return ((DynamicPushFirstFifo *)fifo)->push(size, sequence_number, data);
}

extern "C" byte *
iara_fifo_runtime_DynamicPushFirstFifo_pop(void *fifo, i64 size,
                                           i64 sequence_number) {
  return ((DynamicPushFirstFifo *)fifo)->pop(size, sequence_number);
}
