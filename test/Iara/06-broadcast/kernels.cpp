#include <IaraRuntime/common/Scheduler.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>

std::mutex mutex;

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
}

void c(int val[1]) {
  mutex.lock();
  printf("Broadcast 2! Val = %d\n", val[0]);
  printf("Address = %lu\n", (size_t)&val[0]);
  mutex.unlock();
}

void exec() {

  iara_runtime_init();
  iara_runtime_run_iteration(0);
}
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}