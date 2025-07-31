#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>

std::mutex mutex;

extern "C" {

int original_value;
int *original_address;

int values[4];
int *addresses[4];
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

void c(int val[1]) {
  // Do nothing, just pass through.
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}
}

int main() {
  iara_runtime_exec(exec);
  iara_runtime_wait();

  printf("Original value: %d @%lx\n", original_value, (size_t)original_address);

  auto num_reuses = 0;
  auto num_mistakes = 0;

  for (int i = 0; i < 4; i++) {
    if (values[i] != original_value)
      num_mistakes++;
    printf("Value %d: %d @%lx\n", i, values[i], (size_t)addresses[i]);
    if (original_address == addresses[i])
      num_reuses++;
  }

  printf("There were %d mistakes when copying the value.\n", num_mistakes);

  printf("The value was copied %d time%s and reused %d time%s.\n",
         4 - num_reuses,
         (4 - num_reuses > 1) ? "s" : "",
         num_reuses,
         (num_reuses > 1 ? "s" : ""));

  return 0;
}