#include <stdio.h>

void c();

void foo(int *out) { *out = 2; }
void f(int *in, int *out) { *out = *in * 3; }
void bar(int *in) { printf("%d\n", *in * 5); }

int main() { c(); }