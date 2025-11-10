#include <IaraRuntime/common/Scheduler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

std::mutex m;

bool hit[6] = {0,0,0,0,0,0};

extern "C" void a(int32_t out[2]) {
  static int counter = 0;
  m.lock();
  out[0] = counter++;
  out[1] = counter++;
  m.unlock();
 }



extern "C" void b(const int32_t in[3]) {
  hit[in[0]] = true;
  hit[in[1]] = true;
  hit[in[2]] = true;
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0);
  
}

int main() {
  iara_runtime_exec(exec);

  for (auto i: hit){
    if (!i){
      fprintf(stderr, "Did not receive value %d\n", i);
      exit(1);
    }
  }
  return 0;
}