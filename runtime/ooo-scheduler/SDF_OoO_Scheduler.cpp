#include "Iara/Util/Types.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include <cstdlib>

extern "C" void iara_runtime_alloc(i64 seq, Chunk *chunk) {
  chunk->allocated = (byte *)malloc(chunk->data_size);
}

extern "C" void iara_runtime_dealloc(i64 seq, Chunk *chunk) {
  free(chunk->allocated);
}
