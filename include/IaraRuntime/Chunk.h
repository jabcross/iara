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

// Type that represents a chunk of memory.
struct alignas(64) Chunk {

  byte *allocated = nullptr;
  i64 ooo_offset = 0;
  byte *data;
  i64 data_size;

  Chunk();
  Chunk(i64 ooo_offset) : ooo_offset(ooo_offset) {}

public:
  bool is_released() { return allocated == nullptr; }

  // Returns a portion of the beginning of the chunk, and shrinks this one
  // accordingly
  Chunk take(i64 amount) {
    assert(data_size >= amount);
    Chunk rv{ooo_offset};
    rv.allocated = allocated;
    rv.data = data;
    rv.data_size = amount;
    data += amount;
    data_size -= amount;
    ooo_offset += amount;
    if (data_size == 0) {
      release();
    }
    return rv;
  }

  void release() { *this = Chunk(); }

  static Chunk make_empty() { return Chunk(); }
};

#endif
