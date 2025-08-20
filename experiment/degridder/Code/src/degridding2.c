#define _POSIX_C_SOURCE 199309L
#include "common.h"
#include "constants.h"
#include "degridding.h"
#include "map.h"
#include <assert.h>
#include <fftw3.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef MIN
  #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
  #define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// float2 complex_mult(const float2 z1, const float2 z2) {
//   float2 result;
//   result.x = z1.x * z2.x - z1.y * z2.y;
//   result.y = z1.y * z2.x + z1.x * z2.y;
//   return result;
// }

// void iterator(int *out) {

//   if (NUM_VISIBILITIES % NUM_CHUNK != 0) {
//     printf(
//         "Warning: NUM_VISIBILITIES (%d) is not divisible by NUM_CHUNK
//         (%d)\n", NUM_VISIBILITIES, NUM_CHUNK);
//   }

//   int start = 0;
//   // int num_chunks = NUM_VISIBILITIES / CHUNK;

//   for (int i = 0; i < NUM_CHUNK; i++) {
//     out[i] = start + (i * (NUM_VISIBILITIES / NUM_CHUNK));
//     // printf("out[i] %d\n", out[i]);
//   }

//   struct timespec ts;
//   clock_gettime(CLOCK_REALTIME, &ts);

//   // ts.tv_sec = secondes depuis Epoch
//   // ts.tv_nsec = nanosecondes (0..999,999,999)

//   printf("Time start : %ld.%03ld seconds\n", ts.tv_sec, ts.tv_nsec /
//   1000000);
// }

/*
void std_degridding(int NUM_VISIBILITIES, int GRID_SIZE, int NUM_KERNELS, int
TOTAL_KERNEL_SAMPLES, int OVERSAMPLING_FACTOR, float2* input_grid, float3*
vis_uvw_coords, int2* kernel_supports, float2* kernels, Config* config, float2*
output_visibilities){

        memset(output_visibilities, 0, sizeof(float2) * NUM_VISIBILITIES);

        int grid_center = GRID_SIZE / 2;

        for(int i = 0; i < NUM_VISIBILITIES; ++i){

                // Calculate the w index based on the corrected z coordinate of
the visibilities int w_idx = (int)(SQRT(ABS(vis_uvw_coords[i].z *
config->w_scale)) + 0.5);

                // Retrieve the kernel thickness and offset for the w plane
                int half_support = kernel_supports[w_idx].x;
                int w_offset = kernel_supports[w_idx].y;

                // Calculate the grid position from the UV coordinates
                float2 grid_pos = {.x = vis_uvw_coords[i].x * config->uv_scale,
.y = vis_uvw_coords[i].y * config->uv_scale};

                // Determine if the visibility should be conjugated based on the
z coordinate float conjugate = (vis_uvw_coords[i].z < 0.0) ? -1.0 : 1.0;
                // Initialize the norm of the kernel coefficients for
normalization float comm_norm = 0.f;

                // Iterate over the positions on the y-axis within the kernel
window for(int v = CEIL(grid_pos.y - half_support); v < CEIL(grid_pos.y +
half_support); ++v)
                {

                // Correct the v index if necessary to respect the grid bounds
                        int corrected_v = v;
                        if(v < -grid_center){
                                corrected_v = (-grid_center - v) - grid_center;
                        }
                        else if(v >= grid_center){
                                corrected_v = (grid_center - v) + grid_center;
                        }
                        corrected_v = MAX(-grid_center, Mgrid_center - 1,
corrected_v));

                        // Determine the kernel index on the y-axis
                        int2 kernel_idx;
                        kernel_idx.y = abs((int)ROUND((corrected_v - grid_pos.y)
* OVERSAMPLING_FACTOR));

                        // Iterate over the positions on the x-axis within the
kernel window for(int u = CEIL(grid_pos.x - half_support); u < CEIL(grid_pos.x +
half_support); ++u)
                        {

                                int corrected_u = u;

                                if(u < -grid_center){
                                        corrected_u = (-grid_center - u) -
grid_center;
                                }
                                else if(u >= grid_center){
                                        corrected_u = (grid_center - u) +
grid_center;
                                }
                                corrected_u = MAX(-grid_center, Mgrid_center
- 1, corrected_u));

                                // Determine the kernel index on the x-axis
                                kernel_idx.x = abs((int)ROUND((corrected_u -
grid_pos.x) * OVERSAMPLING_FACTOR));

                                // Calculate the kernel index in the kernel
array int k_idx = w_offset + kernel_idx.y * (half_support + 1) *
OVERSAMPLING_FACTOR + kernel_idx.x;

                                // Retrieve the kernel sample and apply
conjugation if necessary float2 kernel_sample = kernels[k_idx];

                                kernel_sample.y *= conjugate;

                                // Calculate the input grid index to retrieve
the value: we have a 2D array and access it as if it were 1D (I think) int
grid_idx = (corrected_v + grid_center) * GRID_SIZE + (corrected_u +
grid_center);

                                // If the index is out of the grid, consider the
input value as zero float2 input_value;

                                        input_value = input_grid[grid_idx];

                                // Perform the complex product between the grid
value and the kernel sample float2 prod = complex_mult(input_value,
kernel_sample);

                                // Accumulate the norm of the kernel samples for
normalization comm_norm += kernel_sample.x * kernel_sample.x + kernel_sample.y *
kernel_sample.y;

                                // Add the product to the final visibility
result output_visibilities[i].x += prod.x; output_visibilities[i].y += prod.y;
                        }
                }
//    	printf("i:%d",i);

        // Calculate the norm of the kernels for normalization
                comm_norm = sqrt(comm_norm);

                // Normalize the visibilities to avoid values that are too small
                output_visibilities[i].x = comm_norm < 1e-5f ?
output_visibilities[i].x / 1e-5f : output_visibilities[i].x / comm_norm;
                output_visibilities[i].y = comm_norm < 1e-5f ?
output_visibilities[i].x / 1e-5f : output_visibilities[i].y / comm_norm;
        }
    printf("std_degridding\n");
}
// */
// void std_degridding_parallel2(int _GRID_SIZE,
//                               int _NUM_VISIBILITIES,
//                               int _NUM_KERNELS,
//                               int _TOTAL_KERNELS_SAMPLES,
//                               int _OVERSAMPLING_FACTOR,
//                               int _NUM_CHUNK,
//                               float2 *kernels,
//                               int2 *kernel_supports,
//                               float2 *input_grid,
//                               float3 *vis_uvw_coords,
//                               Config *config,
//                               int *iterator,
//                               float2 *output_visibilities) {

//   struct timespec ts;
//   clock_gettime(CLOCK_REALTIME, &ts);

//   printf("Time degrid start: %ld.%03ld seconds\n",
//          ts.tv_sec,
//          ts.tv_nsec / 1000000);

//   memset(output_visibilities,
//          0,
//          sizeof(float2) * (_NUM_VISIBILITIES / _NUM_CHUNK));

//   int grid_center = _GRID_SIZE / 2; // TODO possible to calculate once?

//   for (int i = 0; i < (_NUM_VISIBILITIES / _NUM_CHUNK); ++i) {

//     // Calculate the w index based on the corrected z coordinate of the
//     // visibilities
//     int w_idx =
//         (int)(SQRT(fabs(vis_uvw_coords[i + (*iterator)].z * config->w_scale))
//         +
//               0.5);

//     // Retrieve the kernel thickness and offset for the w plane
//     int half_support = kernel_supports[w_idx].x;
//     int w_offset = kernel_supports[w_idx].y;

//     // Calculate the grid position from the UV coordinates
//     float2 grid_pos = {
//         .x = (float)(vis_uvw_coords[i + (*iterator)].x * config->uv_scale),
//         .y = (float)(vis_uvw_coords[i + (*iterator)].y * config->uv_scale)};

//     // Determine if the visibility should be conjugated based on the z
//     // coordinate
//     float conjugate = (vis_uvw_coords[i + (*iterator)].z < 0.0) ? -1.0 : 1.0;
//     // Initialize the norm of the kernel coefficients for normalization
//     float comm_norm = 0.f;

//     // Iterate over the positions on the y-axis within the kernel window
//     for (int v = CEIL(grid_pos.y - half_support);
//          v < CEIL(grid_pos.y + half_support);
//          ++v) {

//       // Correct the v index if necessary to respect the grid bounds
//       int corrected_v = v;
//       if (v < -grid_center) {
//         corrected_v = (-grid_center - v) - grid_center;
//       } else if (v >= grid_center) {
//         corrected_v = (grid_center - v) + grid_center;
//       }
//       corrected_v = MAX(-grid_center, MIN(grid_center - 1, corrected_v));

//       int kernel_width = (half_support + 1) * _OVERSAMPLING_FACTOR;

//       // Determine the kernel index on the y-axis
//       int2 kernel_idx;
//       kernel_idx.y =
//           abs((int)ROUND((corrected_v - grid_pos.y) * _OVERSAMPLING_FACTOR));
//       kernel_idx.y = MIN(kernel_width - 1, kernel_idx.y);

//       // Iterate over the positions on the x-axis within the kernel window
//       for (int u = CEIL(grid_pos.x - half_support);
//            u < CEIL(grid_pos.x + half_support);
//            ++u) {

//         int corrected_u = u;

//         if (u < -grid_center) {
//           corrected_u = (-grid_center - u) - grid_center;
//         } else if (u >= grid_center) {
//           corrected_u = (grid_center - u) + grid_center;
//         }
//         corrected_u = MAX(-grid_center, MIN(grid_center - 1, corrected_u));

//         // Determine the kernel index on the x-axis
//         kernel_idx.x =
//             abs((int)ROUND((corrected_u - grid_pos.x) *
//             _OVERSAMPLING_FACTOR));
//         kernel_idx.x = MIN(kernel_width - 1, kernel_idx.x);

//         // Calculate the kernel index in the kernel array
//         int k_idx = w_offset +
//                     kernel_idx.y * (half_support + 1) * _OVERSAMPLING_FACTOR
//                     + kernel_idx.x;

//         // Retrieve the kernel sample and apply conjugation if necessary
//         float2 kernel_sample = kernels[k_idx];

//         kernel_sample.y *= conjugate;

//         // Calculate the input grid index to retrieve the value: we have a 2D
//         // array and access it as if it were 1D (I think)
//         int grid_idx = (corrected_v + grid_center) * _GRID_SIZE +
//                        (corrected_u + grid_center);

//         // If the index is out of the grid, consider the input value as zero
//         float2 input_value;

//         input_value = input_grid[grid_idx];

//         // Perform the complex product between the grid value and the kernel
//         // sample
//         float2 prod = complex_mult(input_value, kernel_sample);

//         // Accumulate the norm of the kernel samples for normalization
//         comm_norm += kernel_sample.x * kernel_sample.x +
//                      kernel_sample.y * kernel_sample.y;

//         // Add the product to the final visibility result
//         output_visibilities[i].x += prod.x;
//         output_visibilities[i].y += prod.y;
//       }
//     }

//     // Calculate the norm of the kernels for normalization
//     comm_norm = sqrt(comm_norm);

//     // Normalize the visibilities to avoid values that are too small
//     output_visibilities[i].x = comm_norm < 1e-5f
//                                    ? output_visibilities[i].x / 1e-5f
//                                    : output_visibilities[i].x / comm_norm;
//     output_visibilities[i].y = comm_norm < 1e-5f
//                                    ? output_visibilities[i].y / 1e-5f
//                                    : output_visibilities[i].y / comm_norm;
//   }
//   struct timespec te;
//   clock_gettime(CLOCK_REALTIME, &te);

//   printf(
//       "Time degrid end: %ld.%03ld seconds\n", te.tv_sec, te.tv_nsec /
//       1000000);
// }

void std_degridding_parallel(int grid_size,
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
                             float2 *output_visibilities);

void std_degridding_parallel2(float2 *kernels,
                              int2 *kernel_supports,
                              float2 *input_grid,
                              float3 *vis_uvw_coords,
                              Config *config,
                              int *iterator,
                              float2 *output_visibilities) {

  std_degridding_parallel(GRID_SIZE,
                          NUM_VISIBILITIES,
                          NUM_KERNELS,
                          TOTAL_KERNELS_SAMPLES,
                          OVERSAMPLING_FACTOR,
                          NUM_CHUNK,
                          kernels,
                          kernel_supports,
                          input_grid,
                          vis_uvw_coords,
                          config,
                          iterator,
                          output_visibilities);
}
