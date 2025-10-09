// adapted from tdg-benchs

#include "cholesky.h"
#include <IaraRuntime/common/Scheduler.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <omp.h>
#include <openblas/lapack.h>
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

const int block_size = BLOCK_SIZE;

const int block_size_doubles = block_size * block_size;

const int block_size_bytes = block_size_doubles * sizeof(double);

// #define VERBOSE

void omp_potrf(double *const A) {
  static int INFO;
  static const char L = 'L';
  dpotrf_(&L, &block_size, A, &block_size, &INFO, 1);
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

void blocked_cholesky_sequential(double const *RESTRICT input_matrix,
                                 double *RESTRICT output_matrix) {}

void blocked_cholesky_openmp_for(double const *RESTRICT input_matrix,
                                 double *RESTRICT output_matrix) {}

void blocked_cholesky_openmp_tasks(double const *RESTRICT input_matrix,
                                   double *RESTRICT output_matrix) {}

double const *input_g = NULL;
double *output_g = NULL;

#include "kernel_split_join.inc.h"

void exec() {
  iara_runtime_init();
  iara_runtime_wait();
  iara_runtime_run_iteration(0);
  iara_runtime_wait();
}

void blocked_cholesky_virtual_fifo(double const *RESTRICT input_matrix,
                                   double *RESTRICT output_matrix) {
  input_g = input_matrix;
  output_g = output_matrix;

  iara_runtime_exec(exec);
}

int main_original(int argc, char *argv[]);

double *allocate_matrix() {
  return (double *)malloc(sizeof(double) * matrix_size * matrix_size);
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

typedef void (*BlockedCholeskyImpl)(double const *RESTRICT input_matrix_blocked,
                                    double *RESTRICT output_matrix_blocked);

// Returns wall time of the given function (in milliseconds)
int measure_wall_time(BlockedCholeskyImpl impl,
                      double const *RESTRICT input_matrix_blocked,
                      double *RESTRICT output_matrix_blocked) {

  struct timespec time_start;
  struct timespec time_end;
  clock_gettime(CLOCK_MONOTONIC, &time_start);
  impl(input_matrix_blocked, output_matrix_blocked);
  clock_gettime(CLOCK_MONOTONIC, &time_end);

  return (time_end.tv_sec - time_start.tv_sec) * 1000 +
         (time_end.tv_nsec - time_start.tv_nsec) * 1000000;
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

  char const *scheduler = NULL;
  BlockedCholeskyImpl impl = NULL;

  for (int i = 1; i < argc; i++) {
    if (strncmp("--scheduler=", argv[i], 12) == 0) {
      for (int j = 0; j < NUM_SCHEDULERS; j++) {
        if (strncmp(argv[i] + 12, schedulers[j], 256) == 0) {
          scheduler = schedulers[j];
          impl = scheduler_impls[j];
          break;
        }
      }
    } else {
      fprintf(stderr, "Error: Unrecognized argument: \"%s\"\n", argv[i]);
      exit(1);
    }
  }

  fprintf(stderr, "Chosen scheduler: %s\n", scheduler);

  double *input_matrix_linear = allocate_matrix();
  double *input_matrix_blocked = allocate_matrix();
  double *output_matrix_blocked = allocate_matrix();
  double *output_matrix_linear = allocate_matrix();

  initialize_matrix(input_matrix_linear);
  convert_to_blocks(input_matrix_linear, input_matrix_blocked);

  assert(impl != NULL && "Did not set scheduler correctly");

  int wall_time =
      measure_wall_time(impl, input_matrix_blocked, output_matrix_blocked);

  printf("Wall time: %d ms\n", wall_time);
  return 0;
}

//   int main_original(int argc, char *argv[]) {

//     const double eps = BLAS_dfpinfo(blas_eps);

//     int num_iter = atoi(argv[2]);
//     if (num_iter < 0) {
//       fprintf(stderr, "num_iter must be positive\n");
//       exit(1);
//     }

//     const int n = DIM; // matrix size
//     const int ts = BS; // tile size

//     // #CHECK DYNAMIC_BENCH
//     // printf("ts = %d\n", ts);

//     int check = 1; // check result?

//     // Init matrix
//     initialize_matrix(n, ts, matrix);

//     // Allocate dummy
//     dummy = (double ***)malloc(sizeof(double **) * NB);
//     for (int i = 0; i < NB; i++)
//       dummy[i] = (double **)malloc(sizeof(double *) * NB);
//     for (int i = 0; i < NB; i++)
//       for (int j = 0; j < NB; j++)
//         dummy[i][j] = (double *)calloc(BS * BS, sizeof(double));

//     // Allocate blocked matrix
//     for (int i = 0; i < NB; i++) {
//       for (int j = 0; j < NB; j++) {
//         Ah[i][j] = dummy[i][j];
//       }
//     }

//     for (int i = 0; i < n * n; i++) {
//       original_matrix[i] = matrix[i];
//     }

//     convert_to_blocks(ts, NB, n, (double (*)[n])matrix, Ah);

//     iara_runtime_run_iteration(0);

//     iara_runtime_wait();

//     convert_to_linear(ts, NB, n, Ah, (double (*)[n])matrix);

//     if (check) {
//       const char uplo = 'L';
//       if (check_factorization(n, original_matrix, matrix, n, uplo, eps))
//         check++;
//     }

//     if (check == 2) {
//       printf("Factorization correct\n");
//       // return 0;
//     }

//     // float time = t2;
//     // float gflops = (((1.0 / 3.0) * n * n * n) / ((time) * 1.0e+9));

//     // Print results
// #ifdef VERBOSE
//     printf("============ CHOLESKY RESULTS ============\n");
//     printf("  matrix size:          %dx%d\n", n, n);
//     printf("  block size:           %dx%d\n", ts, ts);
//   #ifndef SEQ
//     printf("  number of threads:    %d\n", omp_get_num_threads());
//   #endif
//     // printf( "  performance (gflops): %f\n", gflops);
//     printf("==========================================\n");
// #else
//     // printf("\n N    BS  #blocks   msecs  Result    performance
//     // (gflops)\n"); printf(" N=%4d  BS=%4d  #BLOCKS=%6d  %9.3fms Res=%2.6f
//     // gflops=%2.6f \n", n, ts, NB*NB, t2*1e3, matrix[n*n-1], gflops);
// #endif
//     return 0;
//   }
