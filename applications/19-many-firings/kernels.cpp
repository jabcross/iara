#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

// Producer fires N times, each emitting a unique value (a monotonically
// increasing counter). The single consumer firing gathers all N values. Because
// firings may run in any order (and on any thread), the gathered values are a
// permutation of 0..N-1 — we validate the multiset, not the order.

#define N 2048

static std::mutex m;
static int counter = 0;
static bool seen[N] = {false};

extern "C" void prod(int32_t out[1]) {
  m.lock();
  int v = counter++;
  m.unlock();
  out[0] = v;
}

extern "C" void cons(const int32_t in[N]) {
  for (int i = 0; i < N; i++) {
    int v = in[i];
    if (v < 0 || v >= N) {
      fprintf(stderr, "value out of range: %d\n", v);
      exit(1);
    }
    if (seen[v]) {
      fprintf(stderr, "duplicate value: %d\n", v);
      exit(1);
    }
    seen[v] = true;
  }
}

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}

int main() {
  iara_runtime_exec(exec);
  for (int i = 0; i < N; i++) {
    if (!seen[i]) {
      fprintf(stderr, "missing value %d\n", i);
      return 1;
    }
  }
  printf("OK %d firings\n", N);
  return 0;
}
