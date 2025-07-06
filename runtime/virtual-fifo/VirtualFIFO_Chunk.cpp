#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"

VirtualFIFO_Chunk VirtualFIFO_Chunk::take_front(i64 amount) {
  assert(data_size >= amount);
  VirtualFIFO_Chunk rv = *this;
  rv.data_size = amount;
  data += amount;
  data_size -= amount;
  virtual_offset += amount;
  if (data_size == 0) {
    release();
  }
  return rv;
}

VirtualFIFO_Chunk VirtualFIFO_Chunk::take_back(i64 amount) {
  assert(data_size >= amount);
  auto back = *this;
  auto front = back.take_front(back.data_size - amount);
  *this = front;
  return back;
}

VirtualFIFO_Chunk VirtualFIFO_Chunk::allocate(i64 size, i64 virtual_offset) {
  // static int x = 1;
  auto allocated = (i8 *)malloc(size);
  // #ifndef IARA_COMPILER
  //     allocated_ptrs[allocated] = x++;
  //     fprintf(stderr, "allocating ptr %d\n", x - 1);
  //     fflush(stderr);
  // #endif
  return VirtualFIFO_Chunk{
      .allocated = allocated,
      .virtual_offset = virtual_offset,
      .data = allocated,
      .data_size = size,
  };
}