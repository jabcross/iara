#include "IaraRuntime/common/Scheduler.h"
#include <cstdio>

void exec() {
  iara_runtime_init();

  iara_runtime_run_iteration(0);
  iara_runtime_run_iteration(1);
}

extern "C" int main() {

  iara_runtime_exec(exec);

  return 0;
}