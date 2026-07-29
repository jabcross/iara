#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// @produce writes 0..n-1 into its n-element output buffer (the buffer is sized
// by the compile-time param n via the topology's `sizes` clause). @consume sums
// the n elements. main asserts the sum is n*(n-1)/2 — i.e. the buffer really was
// n elements wide.

extern "C" {

long g_n = -1;
long g_sum = -1;

void produce(long n, int out[]) {
  for (long i = 0; i < n; i++)
    out[i] = (int)i;
}

void consume(long n, int in[]) {
  long s = 0;
  for (long i = 0; i < n; i++)
    s += in[i];
  g_n = n;
  g_sum = s;
}

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  long expected = g_n * (g_n - 1) / 2;
  printf("n=%ld, buffer sum=%ld (expected %ld)\n", g_n, g_sum, expected);
  if (g_n != 4) {
    fprintf(stderr, "ERROR: expected n=4, got %ld\n", g_n);
    return 1;
  }
  if (g_sum != expected) {
    fprintf(stderr, "ERROR: sum %ld != %ld — output buffer wrong size\n", g_sum,
            expected);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
