#include <IaraRuntime/common/Scheduler.h>
#include <cstdio>
#include <cstdlib>

// @a1 and @a2 each have a `none`-typed output; @c has two `none`-typed inputs
// and no data edge. @c must fire only after BOTH logic tokens arrive. If the
// logic tokens are dropped from @c's firing threshold, @c fires early (before
// @a2, or as a spurious scheduler source) and the check below fails.

static bool ran_a1 = false;
static bool ran_a2 = false;
static int c_fires = 0;

extern "C" void a1() { ran_a1 = true; }
extern "C" void a2() { ran_a2 = true; }

extern "C" void c() {
  // Ordering: c must not run before both producers.
  if (!ran_a1 || !ran_a2) {
    fprintf(stderr,
            "logic dependency violated: c fired before both producers "
            "(a1=%d a2=%d)\n",
            ran_a1, ran_a2);
    exit(1);
  }
  c_fires++;
}

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}

int main() {
  iara_runtime_exec(exec);
  // Count discriminates the threshold: c consumes two logic tokens and must
  // fire EXACTLY once. A broken threshold (logic tokens dropped => threshold 0)
  // makes c a spurious source AND fires it per-token, so c_fires > 1 — the
  // double-free hazard when this shape gates a dealloc.
  if (c_fires != 1) {
    fprintf(stderr, "c fired %d times, expected exactly 1\n", c_fires);
    return 1;
  }
  printf("OK\n");
  return 0;
}
