#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" void a(int32_t out[1]) { out[0] = 10; }
extern "C" void b(int32_t out[1]) { out[0] = 20; }
extern "C" void c(int32_t out[1]) { out[0] = 30; }

extern "C" void sink(const int32_t a_in[1], const int32_t b_in[1],
                     const int32_t c_in[1]) {
  int sum = a_in[0] + b_in[0] + c_in[0];
  if (sum != 60) {
    fprintf(stderr, "bad sum: %d (expected 60)\n", sum);
    exit(1);
  }
  printf("OK %d\n", sum);
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}
