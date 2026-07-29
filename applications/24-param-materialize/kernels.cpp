#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// @produce receives the materialized compile-time param `n` as its first
// (scalar) argument, then its output buffer. @check reads the value back.

extern "C" {

int g_observed = -1;

void produce(long n, int out[1]) { out[0] = (int)n; }

void check(int in[1]) { g_observed = in[0]; }

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  printf("param n observed at kernel: %d (expected 42)\n", g_observed);
  if (g_observed != 42) {
    fprintf(stderr, "ERROR: param not materialized to kernel arg: got %d\n",
            g_observed);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
