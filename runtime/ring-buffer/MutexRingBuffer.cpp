#include "IaraRuntime/ring-buffer/MutexRingBuffer.h"
#include "IaraRuntime/util/DebugPrint.h"
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <llvm/Support/MathExtras.h>
#include <mutex>

// #define IARA_DEBUGPRINT

#ifdef IARA_DEBUGPRINT
  #define DEBUG(x) x

std::mutex debug_mutex;

void printChunkOfInts(i8 *data, size_t data_size) {
  auto size = data_size / sizeof(int);
  auto first = (int *)data;

  for (int i = 0; i < size; i++) {
    debugPrintThreadColor("%d ", first[i]);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

void printChunkOfFloats(i8 *data, size_t data_size) {
  auto size = data_size / sizeof(int);
  auto first = (float *)data;

  for (int i = 0; i < size; i++) {
    debugPrintThreadColor("%0.03f ", first[i]);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

void dumpQueue(MutexRingBuffer *queue, i64 edge_id) {

  i8 *data = queue->m_buffer;
  size_t data_size = queue->m_capacity;

  auto size = data_size / sizeof(float);
  auto first = (float *)data;

  size_t f = queue->m_front - queue->m_buffer;
  size_t b = queue->m_back - queue->m_buffer;
  f /= 4;
  b /= 4;

  debugPrintThreadColor("%ld size %lu capacity %lu F = %lu B = %lu\n",
                        edge_id,
                        queue->m_size / 4,
                        queue->m_capacity / 4,
                        f,
                        b);
  debugPrintThreadColor("[ ");

  if (b < f) {
    fprintf(stderr, "\e[4m");
  }

  for (int i = 0; i < size; i++) {
    if (i == f) {
      fprintf(stderr, "\e[4m");
    }
    if (i == b) {
      fprintf(stderr, "\e[24m");
    }
    debugPrintThreadColor("%0.03f ", first[i]);
  }

  fprintf(stderr, "\e[24m");

  fprintf(stderr, "\n");

  fprintf(stderr, "  ");
  for (int i = 0; i < queue->m_capacity / 4 + 1; i++) {
    if (i == f) {
      debugPrintThreadColor("F = %lu", f);
      break;
    } else
      fprintf(stderr, "  ");
  }
  fprintf(stderr, "\n  ");
  for (int i = 0; i < queue->m_capacity / 4 + 1; i++) {
    if (i == b) {
      debugPrintThreadColor("B = %lu");
      break;
    } else
      fprintf(stderr, "  ");
  }
  fprintf(stderr, "\n");
}

#else
  #define DEBUG(x)
#endif

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
void MutexRingBuffer::growToFit(size_t capacity, i64 edge_id) {
  size_t newcapacity = m_capacity;
  while (newcapacity < capacity) {
    newcapacity += newcapacity;
  }

#ifdef IARA_DEBUGPRINT
  debug_mutex.lock();

  debugPrintThreadColor("   Growing edge %ld from %lu to %lu\n",
                        edge_id,
                        m_capacity / 4,
                        newcapacity / 4);
  debug_mutex.unlock();
#endif

  // no realloc since this also unwraps the buffer

  size_t front_to_buffer_end = m_buffer + m_capacity - m_front;
  i8 *new_ptr = (i8 *)std::aligned_alloc(64, newcapacity);
  assert(new_ptr != nullptr);
  if (m_front + m_size <= m_buffer + m_capacity) {
    memcpy(new_ptr, m_front, m_size);
  } else {
    memcpy(new_ptr, m_front, front_to_buffer_end);
    memcpy(
        new_ptr + front_to_buffer_end, m_buffer, m_size - front_to_buffer_end);
  }
  free(m_buffer);
  m_buffer = m_front = new_ptr;
  m_back = m_front + m_size;
  m_capacity = newcapacity;
  return;
}

// blocks
void MutexRingBuffer::push(i8 *data, size_t size, i64 edge_id) {
  assert(m_buffer != nullptr);

  m_mutex.lock();

#ifdef IARA_DEBUGPRINT
  debug_mutex.lock();

  debugPrintThreadColor("   Pushing %ld floats from edge %ld queue %#016lx "
                        "size %lu capacity %lu\n ",
                        size / 4,
                        edge_id,
                        (size_t)this,
                        m_size / 4,
                        m_capacity / 4);
  printChunkOfFloats(data, size);
  dumpQueue(this, edge_id);
  debug_mutex.unlock();
#endif

  if (m_size + size > m_capacity) {
    growToFit(size + m_size, edge_id);
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

#ifdef IARA_DEBUGPRINT

  debug_mutex.lock();

  dumpQueue(this, edge_id);

  debug_mutex.unlock();

#endif
  m_mutex.unlock();
}

bool MutexRingBuffer::tryPop(i8 *data, size_t size, i64 edge_id) {
  if (!m_mutex.try_lock())
    return false;
  if (m_size < size) {
    m_mutex.unlock();
    return false;
  }

#ifdef IARA_DEBUGPRINT
  debug_mutex.lock();
  debugPrintThreadColor("   Popping %ld floats from edge %ld queue %#016lx "
                        "size %lu capacity %lu\n ",
                        size / 4,
                        edge_id,
                        (size_t)this,
                        m_size / 4,
                        m_capacity / 4);
  dumpQueue(this, edge_id);
  debug_mutex.unlock();

#endif

  size_t front_to_buffer_end = m_buffer + m_capacity - m_front;

  if (size <= front_to_buffer_end) {
    memcpy(data, m_front, size);
    m_front += size;
    m_size -= size;

  } else {

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
  }

#ifdef IARA_DEBUGPRINT
  debug_mutex.lock();

  dumpQueue(this, edge_id);
  debug_mutex.unlock();
#endif

  m_mutex.unlock();
  return true;
}