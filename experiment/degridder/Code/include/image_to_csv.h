#ifndef DEGRIDDER_H
#define DEGRIDDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "stb_image_write.h"

#pragma message("Including image_to_csv.h")

void save_heatmap_png(float2 *image, int size, const char *filename);
void export_image_to_csv(float *image_grid, Config *config);
void export_image_to_csv_input_grid(float2 *image_grid);
void export_image_to_csv_uv_grid(float2 *image_grid);
void export_image_to_csv_uv_grid_shift(float2 *image_grid);
void export_image_to_csv_gridding(float2 *image_grid);
void export_image_to_csv_reconstructed(float2 *image_grid);
void export_image_to_csv_ifft_test(float2 *image_grid);

#ifdef __cplusplus
}
#endif

#endif
