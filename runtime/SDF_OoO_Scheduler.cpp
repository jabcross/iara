#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include <cstdlib>

extern "C" void iara_runtime_alloc_wrapper(i64 seq, std::span<Chunk> data) {
  assert(data.size() == 1);
  data.front().allocated = (byte *)malloc(data.front().data_size());
}

extern "C" void iara_runtime_dealloc_wrapper(i64 seq, std::span<Chunk> data) {
  assert(data.size() == 1);
  free(data.front().allocated);
}
