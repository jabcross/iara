#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>

std::mutex mutex;
std::atomic<int> success_count{0};

extern "C" {

void a(int val[1]) {
  printf("Broadcast!\n");
  printf("Address = %lu\n", (size_t)&val[0]);
  val[0] = 42;
}

void b(int val[1]) {
  mutex.lock();
  printf("Broadcast 1! Val = %d\n", val[0]);
  printf("Address = %lu\n", (size_t)&val[0]);
  mutex.unlock();
  if (val[0] == 42) {
    success_count++;
  } else {
    fprintf(stderr, "ERROR: b received %d instead of 42\n", val[0]);
    exit(1);
  }
}

void c(int val[1]) {
  mutex.lock();
  printf("Broadcast 2! Val = %d\n", val[0]);
  printf("Address = %lu\n", (size_t)&val[0]);
  mutex.unlock();
  if (val[0] == 42) {
    success_count++;
  } else {
    fprintf(stderr, "ERROR: c received %d instead of 42\n", val[0]);
    exit(1);
  }
}

void exec() {

  iara_runtime_init();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();
  
  // Verify both b and c ran successfully
  if (success_count != 2) {
    fprintf(stderr, "ERROR: Expected 2 successful broadcasts, got %d\n", success_count.load());
    exit(1);
  }
  printf("Test passed: Both nodes received broadcast value correctly\n");
}
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}