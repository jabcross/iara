#include <stdio.h>
#include <stdlib.h>

void iara_runtime_init();
void iara_runtime_run_iteration(int64_t graph_iteration);

void a(int val[1]) {
  printf("Hello ");
  val[0] = 42;
}

void b(int val[1]) {
  if (val[0] != 42) {
    exit(1);
  }
  printf("World!\n");
}

int main() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
  return 0;
}