#ifndef IARARUNTIME_RINGBUFFER_CHUNK_H
#define IARARUNTIME_RINGBUFFER_CHUNK_H

#include "Iara/Util/CommonTypes.h"
#include <cassert>
#include <cstdlib>

struct Chunk {

  i8 *allocated = nullptr;
  i8 *data;
  i64 data_size;
};

#endif
