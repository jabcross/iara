#ifndef IARARUNTIME_MEMREF1D_H
#define IARARUNTIME_MEMREF1D_H

#include "Iara/Util/Types.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <regex>
#include <span>

// todo: alignment

// Type that represents a chunk of memory.
struct Chunk {

  i8 *allocated = nullptr;
  i64 ooo_offset = 0;
  i8 *data;
  i64 data_size;

public:
  bool is_released() { return allocated == nullptr; }

  // Returns a portion of the beginning of the chunk, and shrinks this one
  // accordingly
  Chunk take_front(i64 amount) {
    assert(data_size >= amount);
    Chunk rv = *this;
    rv.data_size = amount;
    data += amount;
    data_size -= amount;
    ooo_offset += amount;
    if (data_size == 0) {
      release();
    }
    return rv;
  }

  // Returns a portion of the end of the chunk, and shrinks this one
  // accordingly
  Chunk take_back(i64 amount) {
    assert(data_size >= amount);
    auto back = *this;
    auto front = back.take_front(back.data_size - amount);
    *this = front;
    return back;
  }

  void release() { *this = Chunk(); }

  static Chunk make_empty() { return Chunk(); }
  static Chunk allocate(i64 size, i64 ooo_offset) {
    auto allocated = (i8 *)malloc(size);
    return Chunk{
        .allocated = allocated,
        .ooo_offset = ooo_offset,
        .data = allocated,
        .data_size = size,
    };
  }
};

#endif
