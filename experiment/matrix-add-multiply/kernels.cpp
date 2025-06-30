
#include "IaraRuntime/common/Scheduler.h"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <span>
#include <sstream>
#include <string_view>
#include <valarray>

extern "C" void iara_runtime_init();
extern "C" void iara_runtime_run_iteration(int64_t graph_iteration);

#ifndef MATRIXORDER
  #define MATRIXORDER 4
#endif

constexpr size_t matrix_order = MATRIXORDER;

constexpr size_t matrix_num_elements = matrix_order * matrix_order;

bool write_result = false;

std::mutex print_mutex;

extern float mat_a[1024 * 1024];
extern float mat_b[1024 * 1024];
extern float mat_c[1024 * 1024];

void printMatrix(const float *result) {
  for (size_t row = 0; row < matrix_order; row++) {
    for (size_t col = 0; col < matrix_order; col++) {
      std::cout << result[row * matrix_order + col] << " ";
    }
    std::cout << "\n";
  }
}

extern "C" void matrix_a(float out[matrix_num_elements]) {
  memcpy(out, mat_a, matrix_num_elements * sizeof(float));
}

extern "C" void matrix_b(float out[matrix_num_elements]) {
  memcpy(out, mat_b, matrix_num_elements * sizeof(float));
}

extern "C" void matrix_c(float out[matrix_num_elements]) {
  memcpy(out, mat_c, matrix_num_elements * sizeof(float));
}

extern "C" void add(const float a[1], const float b[1], float c[1]) {

  c[0] = a[0] + b[0];

  // if (write_result) {
  //   print_mutex.lock();
  //   std::cout << a[0] << " + " << b[0] << " = " << c[0] << "\n";
  //   print_mutex.unlock();
  // }
}

extern "C" void multiply(const float a[matrix_num_elements],
                         const float b[matrix_num_elements],
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

  if (write_result) {
    print_mutex.lock();
    std::cout << "AxB:\n";
    printMatrix(c);
    print_mutex.unlock();
  }
}

void writeResult(float result[matrix_num_elements]) {

  std::cout << "Ouput:\n";
  printMatrix(result);
}

float result_matrix[matrix_num_elements];

extern "C" void out(float a[matrix_num_elements]) {
  memcpy(result_matrix, a, matrix_num_elements * sizeof(float));
}

void exec() {
  iara_runtime_init();

  iara_runtime_run_iteration(0);
}

void run_no_scheduler() {
  float ab[matrix_num_elements];
  float abpc[matrix_num_elements];

  multiply(mat_a, mat_b, ab);
  for (size_t i = 0; i < matrix_num_elements; i++) {
    add(&ab[i], &mat_c[i], &abpc[i]);
  }
  out(abpc);
}

bool no_scheduler = false;

extern "C" int main(int argc, char **argv) {

  for (int i = 1; i < argc; i++) {
    if (0 == strcmp(argv[i], "--write-result")) {
      write_result = true;
    }
    if (0 == strcmp(argv[i], "--no-scheduler")) {
      no_scheduler = true;
    }
  }

  if (write_result) {
    print_mutex.lock();
    std::cout << "Output of experiment add_multiply\n";
    std::cout << "Computation: (AxB) + C \n";
    std::cout << "Matrix size: " << matrix_order << "x" << matrix_order << "\n";
    std::cout << "Inputs:\n A:\n";
    printMatrix(mat_a);
    std::cout << "B:\n";
    printMatrix(mat_b);
    std::cout << "C:\n";
    printMatrix(mat_c);
    print_mutex.unlock();
  }

  if (no_scheduler) {
    run_no_scheduler();

  } else {
    // omp_set_num_threads(1);
    iara_runtime_exec(exec);
  }

  if (write_result) {
    print_mutex.lock();
    std::cout << "(AxB)+C:\n";
    printMatrix(result_matrix);
    print_mutex.unlock();
  }

  return 0;
}