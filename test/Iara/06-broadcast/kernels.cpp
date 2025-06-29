#include <IaraRuntime/common/Scheduler.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t mutex;

extern "C" {

void a(int val[1]) {
  printf("Broadcast!\n");
  printf("Address = %lu\n", (size_t)&val[0]);
  val[0] = 42;
}

void b(int val[1]) {
  pthread_mutex_lock(&mutex);
  printf("Broadcast 1! Val = %d\n", val[0]);
  printf("Address = %lu\n", (size_t)&val[0]);
  pthread_mutex_unlock(&mutex);
}

void c(int val[1]) {
  pthread_mutex_lock(&mutex);
  printf("Broadcast 2! Val = %d\n", val[0]);
  printf("Address = %lu\n", (size_t)&val[0]);
  pthread_mutex_unlock(&mutex);
}

void exec() {

  iara_runtime_init();
  iara_runtime_run_iteration(0);
}

int main() {
  pthread_mutex_init(&mutex, NULL);
  iara_runtime_exec(exec);
  return 0;
}