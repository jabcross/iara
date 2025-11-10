#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static std::mutex m;
static int iteration_count = 0;
static int expected_iteration_0[] = {100, 101, 102, 0, 1, 2, 3, 4, 5};
static int expected_iteration_1[] = {6, 7, 8, 0, 1, 2, 3, 4, 5};

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
  int *expected = (iteration_count == 0) ? expected_iteration_0 : expected_iteration_1;
  
  for (int i = 0; i < 9; i++) {
    printf("%d ", in[i]);
    if (in[i] != expected[i]) {
      fprintf(stderr, "\nERROR: Iteration %d, position %d: expected %d, got %d\n",
              iteration_count, i, expected[i], in[i]);
      exit(1);
    }
  }
  printf("\n");
  iteration_count++;
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