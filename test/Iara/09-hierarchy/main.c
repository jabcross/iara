#include <stdint.h>
#include <stdio.h>

void iara_runtime_init();
void iara_runtime_run_iteration(int64_t graph_iteration);

void foo(int *out) { *out = 2; }
void f(int *in, int *out) { *out = *in * 3; }
void bar(int *in) { printf("%d\n", *in * 5); }

int main() {

  iara_runtime_init();

#pragma omp parallel
#pragma omp single
  iara_runtime_run_iteration(0);
  return 0;

 }