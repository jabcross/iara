#include "constants.h"
#define _POSIX_C_SOURCE 199309L
#include "degridding.h"
#include "map.h"
#include <assert.h>
#include <fftw3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef MIN
  #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
  #define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef CLAMP
  #define CLAMP(v, m, M) ((v > M) ? (M) : ((v < m) ? (m) : (v)))
#endif

float2 complex_mult(const float2 z1, const float2 z2) {
  float2 result;
  result.x = z1.x * z2.x - z1.y * z2.y;
  result.y = z1.y * z2.x + z1.x * z2.y;
  return result;
}

void iterator(int *out) {

  if (NUM_VISIBILITIES % NUM_CHUNK != 0) {
    printf(
        "Warning: num_visibilities (%d) is not divisible by num_chunk (%d)\n",
        NUM_VISIBILITIES,
        NUM_CHUNK);
  }

  int start = 0;
  // int num_chunks = num_visibilities / CHUNK;

  for (int i = 0; i < NUM_CHUNK; i++) {
    out[i] = start + (i * (NUM_VISIBILITIES / NUM_CHUNK));
    // printf("out[i] %d\n", out[i]);
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  // ts.tv_sec = secondes depuis Epoch
  // ts.tv_nsec = nanosecondes (0..999,999,999)

  printf("Time start : %ld.%03ld seconds\n", ts.tv_sec, ts.tv_nsec / 1000000);
}

void dump_inputs(int grid_size,
                 int num_visibilities,
                 int num_kernels,
                 int total_kernels_samples,
                 int oversampling_factor,
                 int num_chunk,
                 float2 *kernels,
                 int2 *kernel_supports,
                 float2 *input_grid,
                 float3 *vis_uvw_coords,
                 Config *config,
                 int *iterator,
                 float2 *output_visibilities) {
  FILE *text = fopen("degridder_inputs.txt", "w");
  fprintf(text, "grid_size = %d\n", grid_size);
  fprintf(text, "num_visibilities = %d\n", num_visibilities);
  fprintf(text, "num_kernels = %d\n", num_kernels);
  fprintf(text, "total_kernels_samples = %d\n", total_kernels_samples);
  fprintf(text, "oversampling_factor = %d\n", oversampling_factor);
  fprintf(text, "num_chunk = %d\n", num_chunk);

  fprintf(text, "kernels @ %lu\n", (size_t)kernels);
  for (int i = 0; i < total_kernels_samples; i++) {
    fprintf(text, "   %f %f\n", kernels[i].x, kernels[i].y);
  }
  fprintf(text, "supports @ %lu\n", (size_t)kernel_supports);
  for (int i = 0; i < grid_size * grid_size; i++) {
    fprintf(text, "   %f %f\n", input_grid[i].x, input_grid[i].y);
  }
  fprintf(text, "visibilities @ %lu \n", (size_t)vis_uvw_coords);
  for (int i = 0; i < num_visibilities; i++) {
    fprintf(text,
            "   %f %f %f\n",
            vis_uvw_coords[i].x,
            vis_uvw_coords[i].y,
            vis_uvw_coords[i].z);
  }
  fprintf(text, "config->w_scale: %lf\n", config->w_scale);
  fprintf(text, "config->uv_scale: %lf\n", config->uv_scale);
  fprintf(text, "iterator: %d\n", *iterator);
  fprintf(text, "precision: %lu\n", sizeof(PRECISION));
  fclose(text);
  // exit(1); // Removed to allow function to continue
}

void ensure_no_overlap(int grid_size,
                       int num_visibilities,
                       int num_kernels,
                       int total_kernels_samples,
                       int oversampling_factor,
                       int num_chunk,
                       float2 *kernels,
                       int2 *kernel_supports,
                       float2 *input_grid,
                       float3 *vis_uvw_coords,
                       Config *config,
                       int *iterator,
                       float2 *output_visibilities) {
  char *starts[7];
  char *ends[7];
  starts[0] = (char *)(void *)kernels;
  ends[0] = (char *)(void *)&kernels[total_kernels_samples];
  starts[1] = (char *)(void *)&kernel_supports[0];
  ends[1] = (char *)(void *)&kernel_supports[num_kernels];
  starts[2] = (char *)(void *)input_grid;
  ends[2] = (char *)(void *)&input_grid[grid_size * grid_size];
  starts[3] = (char *)(void *)&vis_uvw_coords[0];
  ends[3] = (char *)(void *)&vis_uvw_coords[num_visibilities];
  starts[4] = (char *)(void *)&config[0];
  ends[4] = (char *)(void *)&config[1];
  starts[5] = (char *)(void *)&iterator[0];
  ends[5] = (char *)(void *)&iterator[1];
  starts[6] = (char *)(void *)&output_visibilities[0];
  ends[6] = (char *)(void *)&output_visibilities[num_visibilities / num_chunk];

  for (int i = 0; i < 7; i++) {
    for (int j = i + 1; j < 7; j++) {
      assert((ends[i] <= starts[j]) || (ends[j] <= starts[i]));
    }
  }
}

void std_degridding_parallel(int grid_size,
                             int num_visibilities,
                             int num_kernels,
                             int total_kernel_samples,
                             int oversampling_factor,
                             int num_chunk,
                             float2 *kernels,
                             int2 *kernel_supports,
                             float2 *input_grid,
                             float3 *vis_uvw_coords,
                             Config *config,
                             int *iterator,
                             float2 *output_visibilities) {

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  printf("Time degrid start: %ld.%03ld seconds\n",
         ts.tv_sec,
         ts.tv_nsec / 1000000);

  memset(
      output_visibilities, 0, sizeof(float2) * (NUM_VISIBILITIES / NUM_CHUNK));

  int grid_center = grid_size / 2; // TODO possible to calculate once?

  for (int i = 0; i < (num_visibilities / num_chunk); ++i) {

    int vis_idx = i + (*iterator);

    vis_idx = CLAMP(vis_idx, 0, num_visibilities - 1);

    assert(vis_idx >= 0 && vis_idx < num_visibilities);

    float3 coords = vis_uvw_coords[vis_idx];

    // Calculate the w index based on the corrected z coordinate of the
    // visibilities
    int w_idx =
        (int)(SQRT(ABS(vis_uvw_coords[i + (*iterator)].z * config->w_scale)) +
              0.5);
    w_idx = CLAMP(w_idx, 0, num_kernels - 1);

    assert(w_idx >= 0 && w_idx < num_kernels);

    // Retrieve the kernel thickness and offset for the w plane
    int half_support = kernel_supports[w_idx].x;
    int w_offset = kernel_supports[w_idx].y;

    // Calculate the grid position from the UV coordinates
    float2 grid_pos = {.x = coords.x * config->uv_scale,
                       .y = coords.y * config->uv_scale};

    // Determine if the visibility should be conjugated based on the z
    // coordinate
    float conjugate = (coords.z < 0.0) ? -1.0 : 1.0;
    // Initialize the norm of the kernel coefficients for normalization
    float comm_norm = 0.f;

    // Iterate over the positions on the y-axis within the kernel window
    for (int v = CEIL(grid_pos.y - half_support);
         v < CEIL(grid_pos.y + half_support);
         ++v) {

      // Correct the v index if necessary to respect the grid bounds
      int corrected_v = v;
      if (v < -grid_center) {
        corrected_v = (-grid_center - v) - grid_center;
      } else if (v >= grid_center) {
        corrected_v = (grid_center - v) + grid_center;
      }
      corrected_v = MAX(-grid_center, MIN(grid_center - 1, corrected_v));

      int kernel_width = (half_support + 1) * oversampling_factor;

      // Determine the kernel index on the y-axis
      int2 kernel_idx;
      kernel_idx.y =
          abs((int)ROUND((corrected_v - grid_pos.y) * oversampling_factor));
      kernel_idx.y = MIN(kernel_width - 1, kernel_idx.y);

      // Iterate over the positions on the x-axis within the kernel window
      for (int u = CEIL(grid_pos.x - half_support);
           u < CEIL(grid_pos.x + half_support);
           ++u) {

        int corrected_u = u;

        if (u < -grid_center) {
          corrected_u = (-grid_center - u) - grid_center;
        } else if (u >= grid_center) {
          corrected_u = (grid_center - u) + grid_center;
        }
        corrected_u = MAX(-grid_center, MIN(grid_center - 1, corrected_u));

        // Determine the kernel index on the x-axis
        kernel_idx.x =
            abs((int)ROUND((corrected_u - grid_pos.x) * oversampling_factor));
        kernel_idx.x = MIN(kernel_width - 1, kernel_idx.x);

        // Calculate the kernel index in the kernel array
        int k_idx = w_offset +
                    kernel_idx.y * (half_support + 1) * oversampling_factor +
                    kernel_idx.x;

        k_idx = CLAMP(k_idx, 0, total_kernel_samples - 1);
        assert(k_idx >= 0 && k_idx < total_kernel_samples);

        // Retrieve the kernel sample and apply conjugation if necessary
        float2 kernel_sample = kernels[k_idx];

        kernel_sample.y *= conjugate;

        // Calculate the input grid index to retrieve the value: we have a 2D
        // array and access it as if it were 1D (I think)
        int grid_idx = (corrected_v + grid_center) * grid_size +
                       (corrected_u + grid_center);

        grid_idx = CLAMP(grid_idx, 0, grid_size * grid_size - 1);
        assert(grid_idx >= 0 && grid_idx < grid_size * grid_size);

        // If the index is out of the grid, consider the input value as zero
        float2 input_value;

        input_value = input_grid[grid_idx];

        // Perform the complex product between the grid value and the kernel
        // sample
        float2 prod = complex_mult(input_value, kernel_sample);

        // Accumulate the norm of the kernel samples for normalization
        comm_norm += kernel_sample.x * kernel_sample.x +
                     kernel_sample.y * kernel_sample.y;

        assert(i >= 0 && i < num_visibilities / num_chunk);

        // Add the product to the final visibility result
        output_visibilities[i].x += prod.x;
        output_visibilities[i].y += prod.y;
      }
    }

    // Calculate the norm of the kernels for normalization
    comm_norm = sqrt(comm_norm);

    // Normalize the visibilities to avoid values that are too small
    output_visibilities[i].x = comm_norm < 1e-5f
                                   ? output_visibilities[i].x / 1e-5f
                                   : output_visibilities[i].x / comm_norm;
    output_visibilities[i].y = comm_norm < 1e-5f
                                   ? output_visibilities[i].y / 1e-5f
                                   : output_visibilities[i].y / comm_norm;
  }
  struct timespec te;
  clock_gettime(CLOCK_REALTIME, &te);

  printf(
      "Time degrid end: %ld.%03ld seconds\n", te.tv_sec, te.tv_nsec / 1000000);
}
