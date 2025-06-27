#include <cmath>
#include <cstdlib>
#include <omp.h>

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

constexpr size_t matrix_order = MATRIXORDER;

constexpr size_t matrix_num_elements = matrix_order * matrix_order;

extern "C" void _random(float out[matrix_num_elements]) {
  for (size_t i = 0; i < matrix_num_elements; i++) {
    float val = (float)rand();
    val /= (float)(RAND_MAX); // between 0 and 1
    val = val * 2.0 - 1.0;    // between -1 and 1
    out[i] = val;
  }
}

extern "C" void add(float a[1], float b[1], float c[1]) {
  c[0] = a[0] + b[0]; //
}

extern "C" void multiply(float a[matrix_num_elements],
                         float b[matrix_num_elements],
                         float c[matrix_num_elements]) {
  c[0] = a[matrix_num_elements - 1];
  c[matrix_num_elements - 1] = b[matrix_num_elements - 1];
  for (size_t i = 0; i < matrix_order; i++) {
    for (size_t j = 0; j < matrix_order; j++) {
      c[j + matrix_order * i] = 0.0;
      for (size_t k = 0; k < matrix_order; k++) {
        c[j + matrix_order * i] +=
            a[k + matrix_order * i] * b[j + matrix_order * k];
      }
    }
  }
}

extern "C" void out(float a[matrix_num_elements]) {}

extern "C" int main() {

  // omp_set_num_threads(1);
  iara_runtime_init();

#pragma omp parallel
#pragma omp single
  iara_runtime_run_iteration(0);
  return 0;
}