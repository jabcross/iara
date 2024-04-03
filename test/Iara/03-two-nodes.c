#include <stdio.h>
#include <stdlib.h>

void __iara_run__();

void a_impl(int val[1]) {
  printf("Hello ");
  val[0] = 42;
}

void b_impl(int val[1]) {
  if (val[0] != 42) {
    exit(1);
  }
  printf("World!\n");
}

int main() {
  __iara_run__();
  return 0;
}