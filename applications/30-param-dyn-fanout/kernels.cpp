#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// F5 regression for the placeholder-broadcast path: a sub-graph @emit produces
// a param-sized dynamic tensor, and @run fans it out to a hierarchical consumer
// (@sink, a sub-graph actor) and a leaf (@check). Canonicalize must insert the
// fan-out broadcast with a placeholder dynamic output (borrow would rebuild the
// hierarchical @sink node pre-flatten and break its signature match); flatten
// inlines @sink; --iara-param-materialize resolves the placeholder to the real
// static type and renames/codegens the copy broadcast. main asserts both
// consumers saw the [1..4] buffer.

extern "C" {

int g_head = -1;
int g_check_ok = 0;

void produce(int out[4]) {
  for (int i = 0; i < 4; i++)
    out[i] = i + 1;
}

void check_head(int in[4], int out[1]) { out[0] = (in[0] == 1) ? 1 : 0; }

void check(int in[4]) {
  g_check_ok = 1;
  for (int i = 0; i < 4; i++) {
    if (in[i] != i + 1) {
      g_check_ok = 0;
      return;
    }
  }
}

void record(int head[1]) { g_head = head[0]; }

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  if (g_head != 1) {
    fprintf(stderr, "ERROR: hierarchical sink saw wrong first element: %d\n",
            g_head);
    return 1;
  }
  if (!g_check_ok) {
    fprintf(stderr, "ERROR: leaf consumer buffer content mismatch\n");
    return 1;
  }
  printf("Test passed: dynamic fan-out resolved through hierarchical sink\n");
  return 0;
}
