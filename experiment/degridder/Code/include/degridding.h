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

void convert_vis_to_csv_test(float2 *output_visibilities,
                             float3 *vis_uvw_coords,
                             Config *config);

void iterator(int *out);

void std_degridding_parallel2(float2 *kernels,
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
