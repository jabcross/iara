#ifndef IARA_RUNTIME_SDF_OOO_Buffer_H
#define IARA_RUNTIME_SDF_OOO_Buffer_H

#include "IaraRuntime/Chunk.h"
#include "Util/types.h"
#include <cstdint>
#include <cstdlib>
#include <memory>

// SDF Out of Order Buffer. Provides a single index space for all SDF edges that
// operate in-place. Many edges may share a single buffer.

struct SDF_OoO_Buffer {
  struct Impl;
  Impl *impl;

  // Gets a pointer to the memory associated with the given FIFO index. This
  // is not the edge ID, it is the number of the FIFO in this buffer chain.
  // This is for writing. Reading should be done with a callback.
  Chunk get_for_write(i64 local_fifo_index, i64 sequence_number, i64 size);

  void register_callback(i64 local_fifo_index, i64 sequence_number, i64 size,
                         void (*callback)(Chunk buffer));

  static SDF_OoO_Buffer *create(i64 first_size, i64 next_sizes, i64 period,
                                i64 *prod_rates, i64 *cons_rates, i64 *delays);
};

#endif
