#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include <span>

// Wrapper format. Expects outputs to contain sizing information.
void iara_distribute(i64 seq, std::span<VirtualFIFO_Chunk> args);