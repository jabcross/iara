#include <stdio.h>
#include <stdlib.h>
#include <IaraRuntime/common/Scheduler.h>

void produce(int data[8]) {
  for (int i = 0; i < 8; i++) {
    data[i] = i;
  }
  printf("Produced: [%d, %d, %d, %d, %d, %d, %d, %d]\n",
         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

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

void consume(int data[8]) {
  printf("Consumed: [%d, %d, %d, %d, %d, %d, %d, %d]\n",
         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

void exec() {
  iara_runtime_init();
  iara_runtime_wait();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_shutdown();
  return 0;
}
