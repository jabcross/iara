#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

extern "C" void a(int32_t out[1]) { out[0] = 4; }

extern "C" void b(const int32_t in[1]) {
  printf("%d ", in[0]);
  if (in[0] == 4) {
    printf("\n");
  }
}

extern "C" void run();

extern "C" int main() {
  run();
  return 0;
}