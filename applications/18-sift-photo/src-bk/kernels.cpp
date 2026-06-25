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
  #define IMAGE_WIDTH 3840
#endif
#ifndef IMAGE_HEIGHT
  #define IMAGE_HEIGHT 2160
#endif
#ifndef VIDEO_FRAMES
  #define VIDEO_FRAMES 1
#endif
#define VIDEO_MAX_FRAMES 256

  int image_width = IMAGE_WIDTH;
  int image_height = IMAGE_HEIGHT;

  fprintf(stderr, "SIFT Feature Detection\n");
  fprintf(stderr, "  Image size: %d x %d\n", image_width, image_height);
  fprintf(stderr, "  Video frames: %d\n", VIDEO_FRAMES);

  // #ifdef SCHEDULER_IARA
  fprintf(stderr, "Compiled scheduler: virtual-fifo-iara\n");
  // #endif

  fprintf(stderr, "Starting SIFT feature detection\n");

  double frame_times[VIDEO_MAX_FRAMES];
  struct timespec total_start, total_end;
  clock_gettime(CLOCK_MONOTONIC, &total_start);

  // Init runtime once (cold allocation on first init)
  // #ifdef SCHEDULER_IARA
  iara_runtime_init();
  // #endif

  for (int f = 0; f < VIDEO_FRAMES; f++) {
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    // Run SIFT pipeline (re-reads input file, overwrites outputs)
    // #ifdef SCHEDULER_IARA
    iara_runtime_run_iteration(0, 0);
    iara_runtime_wait();
    // #endif

    clock_gettime(CLOCK_MONOTONIC, &t2);
    frame_times[f] = (t2.tv_sec - t1.tv_sec) * 1000.0
                   + (t2.tv_nsec - t1.tv_nsec) / 1e6;
    printf("Frame %d: %.3f ms\n", f + 1, frame_times[f]);
  }

  // #ifdef SCHEDULER_IARA
  iara_runtime_shutdown();
  // #endif

  clock_gettime(CLOCK_MONOTONIC, &total_end);
  double wall_time =
      ((double)(total_end.tv_sec - total_start.tv_sec)) +
      ((double)(total_end.tv_nsec - total_start.tv_nsec)) / 1000000000.0;

  printf("Wall time: %lf s\n", wall_time);

  // Steady-state average from last half of frames
  if (VIDEO_FRAMES > 1) {
    int half = VIDEO_FRAMES / 2;
    double steady_avg = 0.0;
    for (int f = half; f < VIDEO_FRAMES; f++)
      steady_avg += frame_times[f];
    steady_avg /= (VIDEO_FRAMES - half);
    printf("Steady-state avg: %.3f ms\n", steady_avg);
  }

  return 0;
}
