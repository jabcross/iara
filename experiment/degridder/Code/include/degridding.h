#ifndef DEGRIDDER_H
#define DEGRIDDER_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

void subtract_image_space(float *measurements, float *estimate, float *result);

void correct_to_finegrid(float3 *vis_uvw_coords,
                         float2 *input_visibilities,
                         Config *config,
                         float2 *output_visibilities,
                         float3 *output_finegrid_vis_coords,
                         int *num_output_visibilities);

/*
void std_degridding(int num_visibilities, int grid_size, int num_kernels, int
total_kernels_samples, int oversampling_factor, float2* input_grid, float3*
vis_uvw_coord, int2* support, float2* kernels, Config* config, float2*
output_visibilities);
*/

void gridding_CPU(float2 *kernel,
                  int2 *supports,
                  float3 *vis_uvw_coord,
                  float2 *vis_uvw,
                  Config *config,
                  float2 *grid_in,
                  float2 *grid);

void convert_vis_to_csv_test(float2 *output_visibilities,
                             float3 *vis_uvw_coords,
                             Config *config);

void iterator(int *out);

void std_degridding_parallel(float2 *kernels,
                             int2 *kernel_supports,
                             float2 *input_grid,
                             float3 *vis_uvw_coords,
                             Config *config,
                             int *iterator,
                             float2 *output_visibilities);

#ifdef __cplusplus
}
#endif

#endif
