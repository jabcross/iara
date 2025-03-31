#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

extern "C" void a(int32_t out[1]) {
  out[0] = 4;
  m.lock();
  fprintf(stderr, "A sent: %d\n", out[0]);
  fflush(stderr);
  m.unlock();
}

extern "C" void b(const int32_t in[1]) {
  m.lock();
  fprintf(stderr, "B received: %d\n", in[0]);
  fflush(stderr);
  printf("%d ", in[0]);
  fflush(stdout);
  m.unlock();
}

extern "C" void run();

extern "C" int main() {
  run();
  return 0;
}