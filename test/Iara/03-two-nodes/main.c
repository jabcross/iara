#include <stdio.h>
#include <stdlib.h>
#include <IaraRuntime/common/Scheduler.h>

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

void exec(){
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}