// adapted from tdg-benchs

#include "cholesky.h"
#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <openblas/lapack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Define size from compilation (-DDIM=2 -DNB=2)
#ifndef DIM
#define DIM (4)
#endif

#ifndef NB
#define NB (4)
#endif

#define BS (DIM / NB)
#define BS_BYTES (BS * BS * sizeof(double))

#define BLOCKS ITEM(0, 1) ITEM(1, 0) ITEM(1, 1)

// #define VERBOSE

void omp_potrf(double *const A, int ts, int ld) {
  static int INFO;
  static const char L = 'L';
  dpotrf_(&L, &ts, A, &ld, &INFO, 1);
}

void kernel_potrf(double *in_A, double *out_A) { //
  omp_potrf(in_A, BS, BS);
  // memcpy(out_A, in_A, BS_BYTES);
}

void omp_trsm(double *A, double *B, int ts, int ld) {
  static char LO = 'L', TR = 'T', NU = 'N', RI = 'R';
  static double DONE = 1.0;
  dtrsm_(&RI, &LO, &TR, &NU, &ts, &ts, &DONE, A, &ld, B, &ld);
}

void kernel_trsm(double *in_A, double *in_B, double *out_B) {
  omp_trsm(in_A, in_B, BS, BS);
  // memcpy(out_B, in_B, BS_BYTES);
}

void omp_syrk(double *A, double *B, int ts, int ld) {
  static char LO = 'L', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  dsyrk_(&LO, &NT, &ts, &ts, &DMONE, A, &ld, &DONE, B, &ld);
}

void kernel_syrk(double *in_A, double *in_B, double *out_B) {
  omp_syrk(in_A, in_B, BS, BS);
  // memcpy(out_B, in_B, BS_BYTES);
}

void omp_gemm(double *A, double *B, double *C, int ts, int ld) {
  static const char TR = 'T', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  dgemm_(&NT, &TR, &ts, &ts, &ts, &DMONE, A, &ld, B, &ld, &DONE, C, &ld);
}

void kernel_gemm(double *in_A, double *in_B, double *in_C, double *out_C) {
  omp_gemm(in_A, in_B, out_C, BS, BS);
  // memcpy(in_C, out_C, BS_BYTES);
}

static double *const matrix = (double[DIM * DIM]){0};
static double *const original_matrix = (double[DIM * DIM]){0};
double ***dummy;
double *Ah[NB][NB];

#define ITEM(i, j) , double *out_##i##_##j

void kernel_split(double *out_0_0 BLOCKS) {

#undef ITEM

#define ITEM(i, j) assert(malloc_usable_size(out_##i##_##j) >= BS_BYTES);

  assert(malloc_usable_size(out_0_0) >= BS_BYTES);
  BLOCKS

#undef ITEM

#define ITEM(i, j) memcpy(out_##i##_##j, Ah[i][j], BS_BYTES);
  memcpy(out_0_0, Ah[0][0], BS_BYTES);
  BLOCKS

#undef ITEM
}

#define ITEM(i, j) , double *in_##i##_##j

void kernel_join(double *in_0_0 BLOCKS) {
#undef ITEM

#define ITEM(i, j) memcpy(Ah[i][j], in_##i##_##j, BS_BYTES);
  memcpy(Ah[0][0], in_0_0, BS_BYTES);
  BLOCKS
#undef ITEM
}

void run();

int main(int argc, char *argv[]) {

  const double eps = BLAS_dfpinfo(blas_eps);

  int num_iter = atoi(argv[2]);
  if (num_iter < 0) {
    fprintf(stderr, "num_iter must be positive\n");
    exit(1);
  }

  const int n = DIM; // matrix size
  const int ts = BS; // tile size

  // #CHECK DYNAMIC_BENCH
  // printf("ts = %d\n", ts);

  int check = 1; // check result?

  // Init matrix
  initialize_matrix(n, ts, matrix);

  // Allocate dummy
  dummy = (double ***)malloc(sizeof(double **) * NB);
  for (int i = 0; i < NB; i++)
    dummy[i] = (double **)malloc(sizeof(double *) * NB);
  for (int i = 0; i < NB; i++)
    for (int j = 0; j < NB; j++)
      dummy[i][j] = (double *)calloc(BS * BS, sizeof(double));

  // Allocate blocked matrix
  for (int i = 0; i < NB; i++) {
    for (int j = 0; j < NB; j++) {
      Ah[i][j] = dummy[i][j];
    }
  }

  for (int i = 0; i < n * n; i++) {
    original_matrix[i] = matrix[i];
  }

  convert_to_blocks(ts, NB, n, (double(*)[n])matrix, Ah);

  for (int i = 0; i < 1000; i++)
    run();

  convert_to_linear(ts, NB, n, Ah, (double(*)[n])matrix);

  if (check) {
    const char uplo = 'L';
    if (check_factorization(n, original_matrix, matrix, n, uplo, eps))
      check++;
  }

  if (check == 2) {
    printf("Factorization correct\n");
    // return 0;
  }

  // float time = t2;
  // float gflops = (((1.0 / 3.0) * n * n * n) / ((time) * 1.0e+9));

  // Print results
#ifdef VERBOSE
  printf("============ CHOLESKY RESULTS ============\n");
  printf("  matrix size:          %dx%d\n", n, n);
  printf("  block size:           %dx%d\n", ts, ts);
#ifndef SEQ
  printf("  number of threads:    %d\n", omp_get_num_threads());
#endif
  printf("  time (s):             %f\n", time);
  // printf( "  performance (gflops): %f\n", gflops);
  printf("==========================================\n");
#else
  // printf("\n N    BS  #blocks   msecs  Result    performance (gflops)\n");
  // printf(" N=%4d  BS=%4d  #BLOCKS=%6d  %9.3fms Res=%2.6f  gflops=%2.6f \n",
  // n, ts, NB*NB, t2*1e3, matrix[n*n-1], gflops);
#endif
  return 0;
}
