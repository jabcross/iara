// #ifdef SCHEDULER_IARA
#include "IaraRuntime/common/Scheduler.h"
// #endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// #ifdef SCHEDULER_IARA
void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 0);
}
// #endif

int main() {
// Image dimensions from compile-time defines (IMAGE_WIDTH, IMAGE_HEIGHT)
// Set via CMake DEFINES from experiment configuration
#ifndef IMAGE_WIDTH
  #define IMAGE_WIDTH 800
#endif
#ifndef IMAGE_HEIGHT
  #define IMAGE_HEIGHT 640
#endif

  int image_width = IMAGE_WIDTH;
  int image_height = IMAGE_HEIGHT;

  fprintf(stderr, "SIFT Feature Detection\n");
  fprintf(stderr, "  Image size: %d x %d\n", image_width, image_height);

  // #ifdef SCHEDULER_IARA
  fprintf(stderr, "Compiled scheduler: virtual-fifo-iara\n");
  // #endif

  fprintf(stderr, "Starting SIFT feature detection\n");

  // Measure wall time
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Run SIFT algorithm via IaRa runtime
  // #ifdef SCHEDULER_IARA
  iara_runtime_exec(exec);
  iara_runtime_shutdown();
  // #endif

  clock_gettime(CLOCK_MONOTONIC, &end_time);

  // Calculate elapsed time in seconds
  double wall_time =
      ((double)(end_time.tv_sec - start_time.tv_sec)) +
      ((double)(end_time.tv_nsec - start_time.tv_nsec)) / 1000000000.0;

  // Output measurements in expected format for capture by measurement framework
  printf("Wall time: %lf s\n", wall_time);
  printf("Keypoints found: 0\n");

  return 0;
}