#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

extern "C" void a(float out[10]) {
  for (int i = 0; i < 20; i += 4) {
    out[i] = 1.0;
    out[i + 1] = 2.0;
    out[i + 2] = 3.0;
    out[i + 3] = 2.0;
  }
}

extern "C" void b(const float in[1], const float feedback_in[2], float out[1],
                  float feedback_out[2]) {
  out[0] = (in[0] + feedback_in[0] + feedback_in[1]) / 3;
  feedback_out[0] = in[0];
  feedback_out[1] = feedback_in[0];
}

extern "C" void c(const float in[1]) {
  m.lock();
  printf("%f ", in[0]);
  fflush(stdout);
  m.unlock();
}

extern "C" void run();

extern "C" int main() {
  run();
  return 0;
}