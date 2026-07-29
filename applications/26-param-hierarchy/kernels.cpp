#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// Two @inner instances (params 7 and 11 after flatten substitution) each run
// @emit, writing their param. @check receives both ordered inputs (x = a = 7,
// y = b = 11) and records them; main asserts (7, 11).

extern "C" {

int g_x = -1;
int g_y = -1;

void emit(long p, int out[1]) { out[0] = (int)p; }

void check(int x[1], int y[1]) {
  g_x = x[0];
  g_y = y[0];
}

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  printf("hierarchy params observed: x=%d, y=%d (expected 7, 11)\n", g_x, g_y);
  if (g_x != 7 || g_y != 11) {
    fprintf(stderr, "ERROR: cross-hierarchy params wrong: got (%d, %d)\n", g_x,
            g_y);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
