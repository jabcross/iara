#ifndef IARARUNTIME_COMMON_IO_H
#define IARARUNTIME_COMMON_IO_H

#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"

// Source provides input data to the dataflow graph
struct IaraSource {
  VirtualFIFO_Chunk chunk;
};

// Sink receives output data from the dataflow graph
struct IaraSink {
  VirtualFIFO_Chunk *chunk_ptr;  // Pointer to where output chunk should be written
};

#endif
