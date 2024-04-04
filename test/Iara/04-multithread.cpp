#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

using namespace std::chrono_literals;

extern "C" void __iara_run__();

extern "C" void a_impl(size_t val[1]) {
  std::this_thread::sleep_for(500ms);
  val[0] = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

extern "C" void b_impl(size_t val[1]) {
  std::this_thread::sleep_for(500ms);
  val[0] = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

extern "C" void c_impl(size_t a[1], size_t b[1]) {
  if (a[0] != b[0]) {
    printf("Ran in different threads.\n");
  }
}

int main() {
  __iara_run__();
  return 0;
}