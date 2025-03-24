#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t mutex;

extern "C" {
void run();

void a(int val[1]) {
  printf("Broadcast!\n");
  val[0] = 42;
}

void b(int val[1]) {
  pthread_mutex_lock(&mutex);
  printf("Broadcast 1! Val = %d\n", val[0]);
  pthread_mutex_unlock(&mutex);
}

void c(int val[1]) {
  pthread_mutex_lock(&mutex);
  printf("Broadcast 2! Val = %d\n", val[0]);
  pthread_mutex_unlock(&mutex);
}

int main() {
  pthread_mutex_init(&mutex, NULL);
  run();
  return 0;
}
}