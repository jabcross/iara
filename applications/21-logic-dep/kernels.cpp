#include <IaraRuntime/common/Scheduler.h>
#include <cstdio>
#include <cstdlib>

// @a has a `none`-typed output; @c has a `none`-typed input; there is no data
// edge between them. If the logic edge fails to gate firing order, @c could
// run before @a (or not be counted at all) — either way the check below
// fails and the process exits nonzero.

static bool ran_a = false;
static bool ran_c = false;

extern "C" void a() { ran_a = true; }

extern "C" void c() {
  if (!ran_a) {
    fprintf(stderr, "logic dependency violated: c fired before a\n");
    exit(1);
  }
  ran_c = true;
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}

int main() {
  iara_runtime_exec(exec);
  if (!ran_c) {
    fprintf(stderr, "c never fired\n");
    return 1;
  }
  printf("OK\n");
  return 0;
}
