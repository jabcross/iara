#ifndef IARARUNTIME_RINGBUFFER_MUTEXRINGBUFFER
#define IARARUNTIME_RINGBUFFER_MUTEXRINGBUFFER

#include "Iara/Util/CommonTypes.h"
#include <condition_variable>
#include <cstddef>
#include <mutex>

// Simple thread-safe ring buffer.
struct MutexRingBuffer {
  i8 *m_buffer = nullptr;
  size_t m_capacity = 0;
  // need size to differentiate full buffer from empty buffer (front == back)
  size_t m_size = 0;
  i8 *m_front = nullptr;
  i8 *m_back = nullptr;
  std::mutex m_mutex;
  std::condition_variable m_cv;

  MutexRingBuffer(size_t capacity);

  void growToFit(size_t size, i64 edge_id);

  void push(i8 *data, size_t size, i64 edge_id);

  bool tryPop(i8 *data, size_t size, i64 edge_id);
};

#endif