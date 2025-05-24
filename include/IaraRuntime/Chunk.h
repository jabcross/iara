#ifndef IARARUNTIME_MEMREF1D_H
#define IARARUNTIME_MEMREF1D_H

#include "Util/types.h"
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
  std::span<byte, std::dynamic_extent> data{};
  //  ChunkPtr parent = nullptr; // prevents parent from being deallocated.
  //  std::atomic<i64> children = 0;
  // Function to call on self if number of children changes from one to zero
  //  void (*on_children_zero)(Chunk &);

  Chunk();
  Chunk(i64 ooo_offset) : ooo_offset(ooo_offset) {}

public:
  bool is_released() { return allocated == nullptr; }

  // Returns a portion of the beginning of the chunk, and shrinks this one
  // accordingly
  Chunk take(i64 size) {
    assert(data.size() >= size);
    Chunk rv{ooo_offset};
    rv.allocated = allocated;
    rv.data = data.subspan(0, size);
    data = data.subspan(size, data.size() - size);
    ooo_offset += size;
    if (data.size() == 0) {
      release();
    }
    return rv;
  }

  void release() { *this = Chunk(); }

  size_t data_size() { return data.size(); }

  static Chunk make_empty() { return Chunk(); }
};

#endif
