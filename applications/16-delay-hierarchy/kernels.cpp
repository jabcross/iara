#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static std::mutex m;
static int iteration_count = 0;

extern "C" void iara_runtime_startup();
extern "C" void iara_runtime_run_iteration(int64_t, int);

extern "C" void a(int32_t out[9]) {
  for (int i = 0; i < 9; i++) {
    out[i] = i;
  }
}

extern "C" void b(const int32_t in[3], int32_t out[3]) {
  out[0] = in[0];
  out[1] = in[1];
  out[2] = in[2];
}

extern "C" void c(const int32_t in[9]) {
  m.lock();
  int32_t expected[9];
  if (iteration_count == 0) {
    // Delay provides 3 zeros, then a's first 6 values fill the rest
    expected[0] = 0; expected[1] = 0; expected[2] = 0;
    expected[3] = 0; expected[4] = 1; expected[5] = 2;
    expected[6] = 3; expected[7] = 4; expected[8] = 5;
  } else {
    // Second iteration: no more delay, a provides all 9 values
    // But shift from previous iteration leaves 3,4,5, then 0,1,2,3,4,5
    // Actually: after iter 0, what's in the fifo?
    // a fires again producing 0-8. b fires 3x consuming 3 each.
    expected[0] = 6; expected[1] = 7; expected[2] = 8;
    expected[3] = 0; expected[4] = 1; expected[5] = 2;
    expected[6] = 3; expected[7] = 4; expected[8] = 5;
  }

  for (int i = 0; i < 9; i++) {
    printf("%d ", in[i]);
    if (in[i] != expected[i]) {
      fprintf(stderr,
              "\nERROR: Iteration %d, position %d: expected %d, got %d\n",
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
  iara_runtime_startup();

  iara_runtime_run_iteration(0, 1);
  iara_runtime_run_iteration(1, 1);

  printf("end of exec\n");
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}
