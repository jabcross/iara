#include <IaraRuntime/common/Scheduler.h>
#include <chrono>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

using namespace std::chrono_literals;

extern "C" void a(size_t val[1]) {
  printf("Arrived on A.\n");
  fflush(stdout);
  std::this_thread::sleep_for(500ms);
  val[0] = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

extern "C" void b(size_t val[1]) {
  printf("Arrived on B.\n");
  fflush(stdout);
  std::this_thread::sleep_for(500ms);
  val[0] = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

extern "C" void c(size_t a[1], size_t b[1]) {
  if (a[0] != b[0]) {
    printf("Ran in different threads.\n");
  } else {
    printf("Ran in same thread.\n");
  }
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}