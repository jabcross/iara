#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static std::mutex m;

extern "C" void a(int32_t out[2]) {
  static int counter = 0;
  m.lock();
  out[0] = counter++;
  out[1] = counter++;
  m.unlock();
}

extern "C" void b(const int32_t in[3]) {
  printf("%d %d %d\n", in[0], in[1], in[2]);
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}