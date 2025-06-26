#ifndef IARARUNTIME_VIRTUALFIFO_CHUNK_H
#define IARARUNTIME_VIRTUALFIFO_CHUNK_H

#include "Iara/Util/CommonTypes.h"
#include <cassert>
#include <cstdlib>

// todo: alignment

// #ifndef IARA_COMPILER

// extern std::unordered_map<void *, int> allocated_ptrs;

// #endif

// Type that represents a chunk of memory.
struct Chunk {

  i8 *allocated = nullptr;
  i64 virtual_offset = 0;
  i8 *data;
  i64 data_size;

public:
  inline bool is_released() { return allocated == nullptr; }

  // Returns a portion of the beginning of the chunk, and shrinks this one
  // accordingly
  Chunk take_front(i64 amount);

  // Returns a portion of the end of the chunk, and shrinks this one
  // accordingly
  Chunk take_back(i64 amount);

  void release() { *this = Chunk(); }

  static Chunk make_empty() { return Chunk(); }
  static Chunk allocate(i64 size, i64 virtual_offset);

  inline bool is_empty() { return data_size == 0; }
};

#endif
