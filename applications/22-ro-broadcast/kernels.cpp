#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>

// All-read-only broadcast oracle. @a writes 42 into its output buffer and
// records that buffer's address. Each @b records the value it read and the
// address of its input. With zero-copy borrow every @b sees @a's original
// address (a reuse) and the value 42 (no mistake).

std::mutex mutex;

extern "C" {

int original_value;
int *original_address;

constexpr int N = 3;
int values[N];
int *addresses[N];
int idx = 0;

void a(int val[1]) {
  val[0] = 42;
  original_value = 42;
  original_address = &val[0];
}

void b(int val[1]) {
  mutex.lock();
  values[idx] = val[0];
  addresses[idx] = &val[0];
  idx++;
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

  printf("Original value: %d @%lx\n", original_value, (size_t)original_address);

  int num_reuses = 0;
  int num_mistakes = 0;
  for (int i = 0; i < N; i++) {
    if (values[i] != original_value)
      num_mistakes++;
    printf("Value %d: %d @%lx\n", i, values[i], (size_t)addresses[i]);
    if (original_address == addresses[i])
      num_reuses++;
  }

  printf("There were %d mistakes when copying the value.\n", num_mistakes);
  printf("The value was copied %d time%s and reused %d time%s.\n",
         N - num_reuses, (N - num_reuses == 1) ? "" : "s",
         num_reuses, (num_reuses == 1) ? "" : "s");

  // Target: all-read-only borrow => 0 copies, N reuses, 0 mistakes.
  if (num_mistakes != 0) {
    fprintf(stderr, "ERROR: expected 0 mistakes, got %d\n", num_mistakes);
    return 1;
  }
  if (num_reuses != N) {
    fprintf(stderr, "ERROR: expected %d reuses (zero-copy), got %d\n", N,
            num_reuses);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
