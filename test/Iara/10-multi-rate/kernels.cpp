#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

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

extern "C" int main() {
  iara_runtime_init(1);
  iara_runtime_run_iteration(0);

  return 0;
}