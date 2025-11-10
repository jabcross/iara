#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <IaraRuntime/common/Scheduler.h>


void a(int *out) { *out = 2; }
void b(int *in, int *out) { *out = *in * 3; }
void c(int *in) { 
  int result = *in * 5;
  printf("%d\n", result);
  // Validate: 2 * 3 * 5 = 30
  if (result != 30) {
    fprintf(stderr, "ERROR: Expected 30, got %d\n", result);
    exit(1);
  }
}


void exec(){
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}