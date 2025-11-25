#include <stdio.h>
#include <stdlib.h>
#include <IaraRuntime/common/Scheduler.h>
#include <IaraRuntime/common/IO.h>
#include <IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h>

void process_a(int data[2]) {
  printf("Process A: [%d, %d]\n", data[0], data[1]);
  data[0] += 1;
  data[1] += 1;
}

void process_b(int data[2]) {
  printf("Process B: [%d, %d]\n", data[0], data[1]);
  data[0] += 1;
  data[1] += 1;
}

void process_c(int data[2]) {
  printf("Process C: [%d, %d]\n", data[0], data[1]);
  data[0] += 1;
  data[1] += 1;
}

void process_d(int data[2]) {
  printf("Process D: [%d, %d]\n", data[0], data[1]);
  data[0] += 1;
  data[1] += 1;
}

void exec() {
  // Allocate external buffer
  int buffer[8] = {0, 1, 2, 3, 4, 5, 6, 7};

  printf("Input: [%d, %d, %d, %d, %d, %d, %d, %d]\n",
         buffer[0], buffer[1], buffer[2], buffer[3],
         buffer[4], buffer[5], buffer[6], buffer[7]);

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

  printf("Output: [%d, %d, %d, %d, %d, %d, %d, %d]\n",
         buffer[0], buffer[1], buffer[2], buffer[3],
         buffer[4], buffer[5], buffer[6], buffer[7]);
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_shutdown();
  return 0;
}
