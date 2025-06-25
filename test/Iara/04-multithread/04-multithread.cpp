#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

using namespace std::chrono_literals;

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

extern "C" void a(size_t val[1]) {
  std::this_thread::sleep_for(500ms);
  val[0] = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

extern "C" void b(size_t val[1]) {
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

int main() {
  iara_runtime_init();

#pragma omp parallel
#pragma omp single
  iara_runtime_run_iteration(0);
  return 0;
}