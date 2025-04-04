#ifndef IARA_PASSES_FIFOSCHEDULER_MEMREF_H
#define IARA_PASSES_FIFOSCHEDULER_MEMREF_H

#include <cstddef>
#include <cstdint>

template <typename T, size_t N> struct MemRefDescriptor {
  T *allocated;
  T *aligned;
  intptr_t offset;
  intptr_t sizes[N];
  intptr_t strides[N];
};

#endif // IARA_PASSES_FIFOSCHEDULER_MEMREF_H