#include "IaraRuntime/ring-buffer/MutexRingBuffer.h"
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <llvm/Support/MathExtras.h>
#include <mutex>

MutexRingBuffer::MutexRingBuffer(size_t p_capacity) {
  // alloc cache line
  size_t _capacity = 64;
  while (_capacity < p_capacity)
    _capacity *= 2;
  m_buffer = (i8 *)std::aligned_alloc(64, _capacity);
  m_capacity = _capacity;
  m_front = m_buffer;
  m_back = m_buffer;
}

// Not thread safe
void MutexRingBuffer::growToFit(size_t capacity) {
  size_t newcapacity = m_size;
  while (newcapacity < capacity) {
    newcapacity *= newcapacity;
  }
  // no realloc since this also unwraps the buffer

  size_t front_to_buffer_end = m_buffer + m_capacity - m_front;
  i8 *new_ptr = (i8 *)std::aligned_alloc(64, newcapacity);
  if (m_front + m_size <= m_buffer + m_capacity) {
    memcpy(new_ptr, m_front, m_size);
  } else {
    memcpy(new_ptr, m_front, front_to_buffer_end);
    memcpy(
        new_ptr + front_to_buffer_end, m_buffer, m_size - front_to_buffer_end);
  }
  free(m_buffer);
  m_buffer = m_front = new_ptr;
  m_back = m_front + capacity;
  m_capacity = newcapacity;
  return;
}

// blocks
void MutexRingBuffer::push(i8 *data, size_t size) {
  assert(m_buffer != nullptr);
  m_mutex.lock();
  if (m_size + size > m_capacity) {
    growToFit(size + m_size);
  }
  if (m_back + size <= m_buffer + m_capacity) {
    memcpy(m_back, data, size);
    m_size += size;
    m_back += size;
  } else {
    auto back_to_buffer_end = m_buffer + m_capacity - m_back;
    memcpy(m_back, data, back_to_buffer_end);
    memcpy(m_buffer, data + back_to_buffer_end, size - back_to_buffer_end);
    m_back = m_buffer + size - back_to_buffer_end;
    m_size += size;
  }
  m_mutex.unlock();
  m_cv.notify_one();
}

// blocks
void MutexRingBuffer::pop(i8 *data, size_t size) {
  std::unique_lock<std::mutex> lk(m_mutex);
  m_cv.wait(lk, [&] { return m_size >= size; });

  size_t front_to_buffer_end = m_buffer + m_capacity - m_front;

  if (size <= front_to_buffer_end) {
    memcpy(data, m_front, size);
    m_front += size;
    m_size -= size;
    lk.unlock();
    return;
  }

  // data wraps around
  size_t right_part = front_to_buffer_end;
  size_t left_part = size - right_part;

  memcpy(data, m_front, right_part);
  memcpy(data + right_part, m_buffer, left_part);

  m_size -= size;
  m_front = m_buffer + left_part;
  if (m_front == m_buffer + m_capacity)
    m_front = m_buffer;

  if (m_size == 0) {
    m_front = m_back = m_buffer;
  }
  lk.unlock();
  // No need to notify (all contended pops should come from the same thread)
  // m_cv.notify_one();
}