#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <omp.h>

std::mutex m;

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

// outputs 0 3 6 0 3 6 0 3 6 ...
extern "C" void a(float out[1]) {
  static int x = 0;
  out[0] = (x % 3) * 3;
  x = (x + 1) % 3;
}

extern "C" void b(const float in[1],
                  const float feedback_in[2],
                  float out[1],
                  float feedback_out[2]) {
  out[0] = (in[0] + feedback_in[0] + feedback_in[1]) / 3;
  feedback_out[0] = in[0];
  feedback_out[1] = feedback_in[0];
}

// expects 0 1 3 3 3 3 3 3 3 3
extern "C" void c(const float in[10]) {
  m.lock();
  for (int i = 0; i < 10; i++) {
    printf("%.0f ", in[i]);
  }
  printf("\n");
  fflush(stdout);
  m.unlock();
}

extern "C" void run();

extern "C" int main() {
  iara_runtime_init();

  omp_set_num_threads(1);

#pragma omp parallel
#pragma omp single
  iara_runtime_run_iteration(0);
  return 0;
}