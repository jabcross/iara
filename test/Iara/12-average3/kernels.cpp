#include <IaraRuntime/common/Scheduler.h>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <omp.h>

std::mutex m;
static int iteration_count = 0;
static float expected_iteration_0[] = {0, 1, 3, 3, 3, 3, 3, 3, 3};
static float expected_iteration_1[] = {3, 3, 3, 3, 3, 3, 3, 3, 3};

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

// outputs 0 3 6 0 3 6 0 3 6 ...
extern "C" void a(float out[9]) {
  for (int i = 0; i < 10; i++) {
    out[i] = (i % 3) * 3;
  }
}

extern "C" void b(const float in[1],
                  const float feedback_in[2],
                  float out[1],
                  float feedback_out[2]) {
  out[0] = (in[0] + feedback_in[0] + feedback_in[1]) / 3;
  feedback_out[1] = in[0];
  feedback_out[0] = feedback_in[1];
}

// expects 0 1 3 3 3 3 3 3 3 (original values in delay are 0)
extern "C" void c(const float in[9]) {
  m.lock();
  float *expected = (iteration_count == 0) ? expected_iteration_0 : expected_iteration_1;
  
  for (int i = 0; i < 9; i++) {
    printf("%.0f ", in[i]);
    if (in[i] != expected[i]) {
      fprintf(stderr, "\nERROR: Iteration %d, position %d: expected %.0f, got %.0f\n",
              iteration_count, i, expected[i], in[i]);
      exit(1);
    }
  }
  printf("\n");
  fflush(stdout);
  iteration_count++;
  m.unlock();
}

void exec() {
  iara_runtime_init();

  iara_runtime_run_iteration(0);
  iara_runtime_run_iteration(1);

  printf("end of exec\n");
}

int main() {

  omp_set_num_threads(1);

  iara_runtime_exec(exec);

  return 0;
}