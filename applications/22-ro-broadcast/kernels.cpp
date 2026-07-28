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

  // Correctness (every mode): the read value is always right => 0 mistakes.
  // Reuse count is a mode property, not correctness: with borrow ON every
  // reader aliases @a's buffer (N reuses, zero-copy); with borrow OFF
  // (priming / copy-all-but-one) the copy path keeps only the first buffer
  // (1 reuse, N-1 copies). Assert against the compiler's actual choice
  // (IARA_BROADCAST_BORROW from iara_runtime_config.h), not env.
#ifdef IARA_BROADCAST_BORROW
  const int expected_reuses = N;
#else
  const int expected_reuses = 1;
#endif
  if (num_mistakes != 0) {
    fprintf(stderr, "ERROR: expected 0 mistakes, got %d\n", num_mistakes);
    return 1;
  }
  if (num_reuses != expected_reuses) {
    fprintf(stderr, "ERROR: expected %d reuse%s, got %d\n", expected_reuses,
            expected_reuses == 1 ? "" : "s", num_reuses);
    return 1;
  }
  printf("Test passed\n");
  return 0;
}
