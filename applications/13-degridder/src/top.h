#ifndef TOP_H
#define TOP_H

#ifndef CPU_VERSION
  #define CPU_VERSION 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

void config_struct_set_up(Config *config_struct);

void load_image_from_file(Config *config, float *output);

// void generate_kernel(int NUM_KERNELS, int OVERSAMPLING_FACTOR, int
// NUM_KERNEL_SUPPORT, int TOTAL_KERNELS_SAMPLES, Config* config,
//     float2* kernels, int2* kernel_supports);

// void degridding_kernel_host_set_up(int TOTAL_KERNELS_SAMPLES, int
// NUM_KERNELS, int OVERSAMPLING_FACTOR, Config *config, float2 *kernels,
// int2 *kernel_supports);
void generate_kernel(float2 *kernels, int2 *kernel_supports);
void save_degridding_kernels_to_csv(float2 *degridding_kernels);
void save_degridding_kernel_supports_to_csv(int2 *degridding_kernel_supports);

float kaiser_bessel(float x, float beta);
float I0(float x);
float pswf(float x);

double randn(double mean, double stddev);

void visibility_host_set_up(Config *config, float3 *vis_uvw_coords);

void visibility_host_set_up2(Config *config, float3 *vis_uvw_coords);

void sink(float2 *measured_vis);
void save_visibilities_to_csv(const char *filename, float3 *vis_uvw_coords);

void handle_file_error(FILE *file, const char *message);

void convert_vis_to_csv(PRECISION2 *output_visibilities,
                        PRECISION3 *vis_uvw_coords,
                        Config *config);

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
