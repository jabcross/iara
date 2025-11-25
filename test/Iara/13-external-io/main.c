#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IaraRuntime/common/Scheduler.h>
#include <IaraRuntime/common/IO.h>
#include <IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h>

void process(int data[4]) {
  for (int i = 0; i < 4; i++) {
    data[i] *= 2;
  }
}

void exec() {
  // Allocate external input/output buffer
  int buffer[4] = {1, 2, 3, 4};

  printf("Input: [%d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3]);

  // Create input source chunk (external memory, don't free)
  VirtualFIFO_Chunk input_chunk = VirtualFIFO_Chunk::from_external(
      (int8_t*)buffer,
      sizeof(buffer),
      0
  );

  IaraSource source = {.chunk = input_chunk};

  // Create output sink
  VirtualFIFO_Chunk output_chunk;
  IaraSink sink = {.chunk_ptr = &output_chunk};

  // Set I/O: 1 source (inout), 1 sink (out)
  iara_runtime_set_io(2, &source, &sink);

  iara_runtime_init();
  iara_runtime_wait();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();

  printf("Output: [%d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_shutdown();
  return 0;
}
