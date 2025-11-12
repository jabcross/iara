// adapted from tdg-benchs

#include "cholesky.h"
// adapted from tdg-benchs

#include "cholesky.h"
#ifdef SCHEDULER_IARA
  #include <IaraRuntime/common/Scheduler.h>
#endif
#include <assert.h>
// #include <climits>
// #include <errno.h>
#include <lapack.h>
#include <malloc.h>
#include <omp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef NUM_BLOCKS
  #error "MUST DEFINE NUM_BLOCKS"
#endif

const int num_blocks = NUM_BLOCKS;

#ifndef MATRIX_SIZE
  #error "MUST DEFINE MATRIX SIZE"
#endif

const int matrix_size = MATRIX_SIZE;

#ifndef BLOCK_SIZE
  #define BLOCK_SIZE (MATRIX_SIZE / NUM_BLOCKS)
#endif

const int block_size = BLOCK_SIZE; // side of the square

const int block_size_doubles = block_size * block_size; // area of the square

const int block_size_bytes = block_size_doubles * sizeof(double);

// #define VERBOSE

void omp_potrf(double *inout_A) {
  static int INFO;
  static const char L = 'L';
  dpotrf_(&L, &block_size, inout_A, &block_size, &INFO, 1);
}

void kernel_potrf(double *inout_A) { //
  omp_potrf(inout_A);
  // memcpy(out_A, in_A, BS_BYTES);
}

void omp_trsm(double *A, double *B) {
  static char LO = 'L', TR = 'T', NU = 'N', RI = 'R';
  static double DONE = 1.0;
  int bs = block_size;
  dtrsm_(&RI, &LO, &TR, &NU, &bs, &bs, &DONE, A, &bs, B, &bs);
}

void kernel_trsm(double *in_A, double *inout_B) {
  omp_trsm(in_A, inout_B);
  // memcpy(out_B, in_B, BS_BYTES);
}

void omp_syrk(double *A, double *B) {
  static char LO = 'L', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  int bs = block_size;
  dsyrk_(&LO, &NT, &bs, &bs, &DMONE, A, &bs, &DONE, B, &bs);
}

void kernel_syrk(double *in_A, double *inout_B) {
  omp_syrk(in_A, inout_B);
  // memcpy(out_B, in_B, BS_BYTES);
}

void omp_gemm(double *A, double *B, double *C) {
  static const char TR = 'T', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  int bs = block_size;
  dgemm_(&NT, &TR, &bs, &bs, &bs, &DMONE, A, &bs, B, &bs, &DONE, C, &bs);
}

void kernel_gemm(double *in_A, double *in_B, double *inout_C) {
  omp_gemm(in_A, in_B, inout_C);
  // memcpy(in_C, out_C, BS_BYTES);
}

void add_to_diag(double *matrix, const double alpha) {
  for (int i = 0; i < matrix_size; i++)
    matrix[i + i * matrix_size] += alpha;
}

void initialize_matrix(double *matrix) {
  int ISEED[4] = {0, 0, 0, 1};
  int intONE = 1;

#ifdef VERBOSE
  printf("Initializing matrix with random values ...\n");
#endif

  // diagonal first
  for (int i = 0, e = matrix_size * matrix_size; i < e; i += matrix_size) {
    dlarnv_(&intONE, &ISEED[0], &matrix_size, &matrix[i]);
  }

  for (int i = 0; i < matrix_size; i++) {
    for (int j = 0; j < matrix_size; j++) {
      matrix[j * matrix_size + i] =
          matrix[j * matrix_size + i] + matrix[i * matrix_size + j];
      matrix[i * matrix_size + j] = matrix[j * matrix_size + i];
    }
  }

  add_to_diag(matrix, (double)matrix_size);
}

double *blockAt(double *matrix, int row, int col) {
  return &matrix[(row * num_blocks + col) * block_size_doubles];
}

void blocked_cholesky_sequential(double *inout_matrix) {
  {
    {
      for (int k = 0; k < num_blocks; k++) {

        omp_potrf(blockAt(inout_matrix, k, k));

        for (int i = k + 1; i < num_blocks; i++) {
          omp_trsm(blockAt(inout_matrix, k, k), blockAt(inout_matrix, k, i));
        }

        // Update trailing matrix
        for (int l = k + 1; l < num_blocks; l++) {
          for (int j = k + 1; j < l; j++) {
            omp_gemm(blockAt(inout_matrix, k, l),
                     blockAt(inout_matrix, k, j),
                     blockAt(inout_matrix, j, l));
          }
          omp_syrk(blockAt(inout_matrix, k, l), blockAt(inout_matrix, l, l));
        }
      }
    }
  }
}

void blocked_cholesky_openmp_for(double *inout_matrix_blocked) {}

void blocked_cholesky_openmp_tasks(double *inout_matrix) {
#pragma omp parallel
#pragma omp single
  {
    {
      for (int k = 0; k < num_blocks; k++) {

#pragma omp task depend(inout : *blockAt(inout_matrix, k, k))
        omp_potrf(blockAt(inout_matrix, k, k));

        for (int i = k + 1; i < num_blocks; i++) {
#pragma omp task depend(in : *blockAt(inout_matrix, k, k))                     \
    depend(inout : *blockAt(inout_matrix, k, i))
          omp_trsm(blockAt(inout_matrix, k, k), blockAt(inout_matrix, k, i));
        }

        // Update trailing matrix
        for (int l = k + 1; l < num_blocks; l++) {
          for (int j = k + 1; j < l; j++) {
#pragma omp task depend(in : *blockAt(inout_matrix, k, k))                     \
    depend(in : *blockAt(inout_matrix, k, j))                                  \
    depend(inout : *blockAt(inout_matrix, j, l))
            omp_gemm(blockAt(inout_matrix, k, l),
                     blockAt(inout_matrix, k, j),
                     blockAt(inout_matrix, j, l));
          }
#pragma omp task depend(in : *blockAt(inout_matrix, k, l))                     \
    depend(inout : *blockAt(inout_matrix, l, l))
          omp_syrk(blockAt(inout_matrix, k, l), blockAt(inout_matrix, l, l));
        }
      }
    }
  }
}

double const *input_g = NULL;
double *output_g = NULL;

#ifdef SCHEDULER_IARA
  #include "kernel_split_join.inc.h"

void exec() {
  iara_runtime_init();
  iara_runtime_wait();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();
}

void blocked_cholesky_virtual_fifo(double *inout_matrix_blocked) {
  input_g = inout_matrix_blocked;
  output_g = inout_matrix_blocked;

  iara_runtime_exec(exec);
}
#else
void blocked_cholesky_virtual_fifo(double *inout_matrix_blocked) {
  fprintf(stderr, "ERROR: virtual-fifo scheduler not compiled in this build\n");
  exit(1);
}
#endif

int main_original(int argc, char *argv[]);

void print_matrix(double *matrix) {

  // get biggest number

  int size = 10;

  printf("matrix_size = %d\n", matrix_size);

  // exit(1);

  printf("╔");
  for (int i = 1, e = matrix_size * (size + 1); i < e; i++) {
    printf("═");
  }
  printf("╗\n");

  for (int row = 0; row < matrix_size; row++) {
    printf("║");
    for (int col = 0; col < matrix_size; col++) {
      printf("% 10.3f", matrix[row * matrix_size + col]);
      if (col == matrix_size - 1)
        continue;
      if ((col + 1) % block_size == 0) {
        printf("│");
      } else {
        printf(" ");
      }
    }
    printf("║\n");
    if (row == matrix_size - 1)
      continue;
    if ((row + 1) % block_size == 0) {
      printf("╟");
      for (int col = 0; col < matrix_size; col++) {
        printf("──────────");
        if (col == matrix_size - 1)
          continue;
        if ((col + 1) % block_size == 0) {
          printf("┼");
        } else {
          printf("─");
        }
      }
      printf("╢\n");
    }
  }
  printf("╚");
  for (int i = 1, e = matrix_size * (size + 1); i < e; i++) {
    printf("═");
  }
  printf("╝\n");
}

double *allocate_matrix() {
  return (double *)calloc(matrix_size * matrix_size, sizeof(double));
}

static void convert_to_blocks(double const *RESTRICT input_linear_matrix,
                              double *RESTRICT output_blocked_matrix) {

  int matrix_size = num_blocks * block_size;

  double *out = output_blocked_matrix;
  for (int b_i = 0; b_i < num_blocks; b_i++) {
    for (int b_j = 0; b_j < num_blocks; b_j++) {
      for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
          int line = b_i * block_size + i;
          int column = b_j * block_size + j;
          double value = input_linear_matrix[line * matrix_size + column];
          *out = value;
          out++;
        }
      }
    }
  }
}

static void convert_to_linear(double const *RESTRICT input_blocked_matrix,
                              double *RESTRICT output_linear_matrix) {

  int matrix_size = num_blocks * block_size;

  double const *in = input_blocked_matrix;
  for (int b_i = 0; b_i < num_blocks; b_i++) {
    for (int b_j = 0; b_j < num_blocks; b_j++) {
      for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
          int line = b_i * block_size + i;
          int column = b_j * block_size + j;
          output_linear_matrix[line * matrix_size + column] = *in;
          in++;
        }
      }
    }
  }
}

typedef void (*BlockedCholeskyImpl)(double *inout_matrix_blocked);

// Returns wall time of the given function (in milliseconds)
double measure_wall_time(BlockedCholeskyImpl impl,
                         double *inout_matrix_blocked) {

  struct timespec time_start;
  struct timespec time_end;
  clock_gettime(CLOCK_MONOTONIC, &time_start);
  impl(inout_matrix_blocked);

  clock_gettime(CLOCK_MONOTONIC, &time_end);

  printf("Start time: %20lu.%09lu\n", time_start.tv_sec, time_start.tv_nsec);
  printf("  End time: %20lu.%09lu\n", time_end.tv_sec, time_end.tv_nsec);
  printf(" Wall time: %20lu.%09lu\n",
         time_end.tv_sec - time_start.tv_sec,
         time_end.tv_nsec - time_start.tv_nsec);

  double wall_time =
      ((double)(time_end.tv_sec - time_start.tv_sec)) +
      ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000000L;

  return wall_time;
}

#define NUM_SCHEDULERS 4

static char const *const sequential = "sequential";
static char const *const omp_for = "omp-for";
static char const *const omp_task = "omp-task";
static char const *const virtual_fifo = "virtual-fifo";

char const *schedulers[] = {sequential, omp_for, omp_task, virtual_fifo};

BlockedCholeskyImpl scheduler_impls[] = {&blocked_cholesky_sequential,
                                         &blocked_cholesky_openmp_for,
                                         &blocked_cholesky_openmp_tasks,
                                         &blocked_cholesky_virtual_fifo};

int main(int argc, char *argv[]) {

  fprintf(stderr, "matrix size: %d\n", matrix_size);
  fprintf(stderr, "num_blocks: %d\n", num_blocks);
  fprintf(stderr, "block size: %d\n", block_size);
  fprintf(stderr, "block_size_bytes: %d\n", block_size_bytes);

  // Scheduler is selected at compile time
  BlockedCholeskyImpl impl = NULL;
  char const *scheduler = NULL;

#if defined(SCHEDULER_SEQUENTIAL)
  impl = &blocked_cholesky_sequential;
  scheduler = "sequential";
#elif defined(SCHEDULER_OMP_FOR)
  impl = &blocked_cholesky_openmp_for;
  scheduler = "omp-for";
#elif defined(SCHEDULER_OMP_TASK)
  impl = &blocked_cholesky_openmp_tasks;
  scheduler = "omp-task";
#elif defined(SCHEDULER_IARA)
  impl = &blocked_cholesky_virtual_fifo;
  scheduler = "virtual-fifo";
#else
  #error                                                                       \
      "Must define one of: SCHEDULER_SEQUENTIAL, SCHEDULER_OMP_FOR, SCHEDULER_OMP_TASK, SCHEDULER_IARA"
#endif

  fprintf(stderr, "Compiled scheduler: %s\n", scheduler);

  double *input_matrix_linear = allocate_matrix();
  double *inout_matrix_blocked = allocate_matrix();
  double *output_matrix_linear = allocate_matrix();

  initialize_matrix(input_matrix_linear);

  printf("Original matrix:\n");
  // print_matrix(input_matrix_linear);

  convert_to_blocks(input_matrix_linear, inout_matrix_blocked);

  assert(impl != NULL && "Did not set scheduler correctly");

  fprintf(stderr, "Starting measurement\n");

  double wall_time = measure_wall_time(impl, inout_matrix_blocked);

  convert_to_linear(inout_matrix_blocked, output_matrix_linear);

  printf("Blocked cholesky output:\n");
  // print_matrix(output_matrix_linear);

  printf("Wall time: %lf s\n", wall_time);
  return 0;
}
