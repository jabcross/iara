#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static std::mutex m;

extern "C" void a(int32_t out[9]) {
  m.lock();
  for (int i = 0; i < 9; i++) {
    out[i] = i;
  }
  m.unlock();
}

extern "C" void b(const int32_t in[3], int32_t out[3]) {
  out[0] = in[0];
  out[1] = in[1];
  out[2] = in[2];
}

extern "C" void c(const int32_t in[9]) {
  for (int i = 0; i < 9; i++) {
    printf("%d ", in[i]);
  }
  printf("\n");
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();
  printf("\n");
  iara_runtime_run_iteration(1);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}