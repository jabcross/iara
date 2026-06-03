#include <IaraRuntime/common/Scheduler.h>
#include <cstdio>
#include <cstdlib>

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration,
                                           int wait_for_tasks);

// Produces 96 floats: out[i] = i.
extern "C" void src(float out[96]) {
  for (int i = 0; i < 96; i++)
    out[i] = (float)i;
}

// copyA / copyB / barrier are plain memmove kernels (out[i] = in[i]).
// `barrier` stands in for BarrierTranspose2x: a non-in-place node whose output
// buffer is a fresh allocation consumed downstream by copyB.
extern "C" void copyA(const float in[16], float out[16]) {
  for (int i = 0; i < 16; i++)
    out[i] = in[i];
}
extern "C" void barrier(const float in[16], float out[16]) {
  for (int i = 0; i < 16; i++)
    out[i] = in[i];
}
extern "C" void copyB(const float in[16], float out[16]) {
  for (int i = 0; i < 16; i++)
    out[i] = in[i];
}

// Validates the data round-tripped through the whole chain unchanged. A double-
// allocated block hands copyB / sink a fresh (wrong) buffer, so the check fails
// even when no crash occurs.
extern "C" void sink(const float in[96]) {
  int ok = 0;
  for (int i = 0; i < 96; i++) {
    if (in[i] != (float)i) {
      fprintf(stderr, "MISMATCH at %d: expected %d got %.0f\n", i, i, in[i]);
      exit(1);
    }
    ok++;
  }
  printf("OK %d\n", ok);
  fflush(stdout);
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}

int main() {
  iara_runtime_exec(exec);
  return 0;
}
