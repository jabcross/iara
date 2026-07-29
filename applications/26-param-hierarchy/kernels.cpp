#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>

// Two @inner instances (params 7 and 11 after flatten substitution) each run
// @emit, which writes its param into its output buffer. Two @sink instances
// record the values; main asserts the observed set is {7, 11}. Firing order is
// not guaranteed, so the set is checked order-independently.

extern "C" {

std::mutex mutex;
int observed[2] = {-1, -1};
int idx = 0;

void emit(long p, int out[1]) { out[0] = (int)p; }

void sink(int in[1]) {
  mutex.lock();
  if (idx < 2)
    observed[idx++] = in[0];
  mutex.unlock();
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  int a = observed[0], b = observed[1];
  printf("hierarchy params observed: %d, %d (expected {7, 11})\n", a, b);
  bool ok = (a == 7 && b == 11) || (a == 11 && b == 7);
  if (!ok) {
    fprintf(stderr, "ERROR: cross-hierarchy params wrong: got {%d, %d}\n", a, b);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
