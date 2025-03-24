#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

extern "C" void a(int32_t out[2]) {
  static int counter = 0;
  m.lock();
  out[0] = counter++;
  out[1] = counter++;
  m.unlock();
}

extern "C" void b(const int32_t in[3]) {
  if (in[1] - in[0] != 1 or in[2] - in[1] != 1) {
    printf("Sequence error.");
    exit(1);
  }
  printf("%d %d %d\n", in[0], in[1], in[2]);
}

extern "C" void run();

extern "C" int main() {
  run();
  return 0;
}