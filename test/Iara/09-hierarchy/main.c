#include <stdint.h>
#include <stdio.h>
#include <IaraRuntime/common/Scheduler.h>


void foo(int *out) { *out = 2; }
void f(int *in, int *out) { *out = *in * 3; }
void bar(int *in) { printf("%d\n", *in * 5); }


void exec(){
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}