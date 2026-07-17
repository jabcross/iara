#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>

// Multi-rate all-read-only broadcast oracle. @a writes [10,20] into its 2-int
// output and records that buffer's base address. @b reads it once (mult=1); @c
// reads it 2 ints at a time over 3 firings (mult=3). With zero-copy borrow every
// read aliases @a's buffer: @b sees base, and @c sees base every firing (the
// toroidal wrap folds offsets 0,8,16 back to 0). All values must read [10,20].

std::mutex mutex;

extern "C" {

int *original_address;

constexpr int N = 4; // 1 read by @b + 3 reads by @c
int values0[N];      // first int of each read (expect 10)
int values1[N];      // second int of each read (expect 20)
int *addresses[N];
int idx = 0;

void a(int val[2]) {
  val[0] = 10;
  val[1] = 20;
  original_address = &val[0];
}

static void record(int val[2]) {
  mutex.lock();
  if (idx < N) {
    values0[idx] = val[0];
    values1[idx] = val[1];
    addresses[idx] = &val[0];
    idx++;
  }
  mutex.unlock();
}

void b(int val[2]) { record(val); }
void c(int val[2]) { record(val); }

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  printf("Original @%lx\n", (size_t)original_address);

  int num_reuses = 0;
  int num_mistakes = 0;
  for (int i = 0; i < N; i++) {
    if (values0[i] != 10 || values1[i] != 20)
      num_mistakes++;
    printf("Read %d: [%d,%d] @%lx\n", i, values0[i], values1[i],
           (size_t)addresses[i]);
    if (original_address == addresses[i])
      num_reuses++;
  }

  printf("There were %d mistakes when reading the value.\n", num_mistakes);
  printf("The value was copied %d time%s and reused %d time%s.\n",
         N - num_reuses, (N - num_reuses == 1) ? "" : "s", num_reuses,
         (num_reuses == 1) ? "" : "s");

  // Target: multi-rate all-read-only borrow => 0 copies, N reuses, 0 mistakes.
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
