#include <stdio.h>
#include <stdlib.h>

void run();

void a(int val[1]) {
  printf("Hello ");
  val[0] = 42;
}

void b(int val[1]) {
  if (val[0] != 42) {
    exit(1);
  }
  printf("World!\n");
}

int main() {
  run();
  return 0;
}