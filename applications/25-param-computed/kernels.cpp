#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// @produce receives the SCCP-folded computed parameter `area` = (rows+1)*cols
// as its scalar argument. @check reads it back; main asserts it folded to 42.

extern "C" {

int g_observed = -1;

void produce(long area, int out[1]) { out[0] = (int)area; }

void check(int in[1]) { g_observed = in[0]; }

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  printf("computed area observed at kernel: %d (expected 42)\n", g_observed);
  if (g_observed != 42) {
    fprintf(stderr, "ERROR: computed param did not fold to 42: got %d\n",
            g_observed);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
