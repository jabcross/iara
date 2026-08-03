#include <IaraRuntime/common/Scheduler.h>
#include <stdio.h>

// @produce writes [10, 20] into a fixed 2-element buffer. @consume reads
// k*2 elements (k is the compile-time param): the edge's out size is larger
// than the producer's buffer, so IaRa's toroidal multi-rate read wraps
// around, presenting k back-to-back copies of [10, 20]. main asserts the
// read-back values are exactly k repetitions of [10, 20].

extern "C" {

long g_k = -1;
int g_values[64];

void produce(int out[2]) {
  out[0] = 10;
  out[1] = 20;
}

void consume(long k, int in[]) {
  g_k = k;
  for (long i = 0; i < k * 2; i++)
    g_values[i] = in[i];
}

void exec() {
  iara_runtime_startup();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  if (g_k != 3) {
    fprintf(stderr, "ERROR: expected k=3, got %ld\n", g_k);
    return 1;
  }
  for (long i = 0; i < g_k; i++) {
    if (g_values[2 * i] != 10 || g_values[2 * i + 1] != 20) {
      fprintf(stderr, "ERROR: replication %ld mismatch: got [%d, %d]\n", i,
              g_values[2 * i], g_values[2 * i + 1]);
      return 1;
    }
  }
  printf("Test passed: k=%ld, %ld replications of [10, 20] read back "
         "correctly\n",
         g_k, g_k);
  return 0;
}
