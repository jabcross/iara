#ifndef FFT_RUN_H
#define FFT_RUN_H

//#include <cufft.h>
//#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif
	#include "preesm.h"
    #include <fftw3.h>

	void CUFFT_EXECUTE_INVERSE_C2C_actor(int GRID_SIZE, float2 *uv_grid_in, float2 *uv_grid_out);


	void fft_shift_complex_to_complex_actor(int GRID_SIZE, float2 *uv_grid_in, float2 *uv_grid_out);

	void CUFFT_EXECUTE_FORWARD_C2C_actor(int GRID_SIZE, IN float2* uv_grid_in, OUT float2* uv_grid_out);

	void fft_shift_real_to_complex_actor(int GRID_SIZE, float *input_grid, Config *config, float2 *fourier);
	void fft_shift_complex_to_real_actor(int GRID_SIZE, float2 *uv_grid, float * dirty_image);


#ifdef __NVCC__
	__global__ void fft_shift_complex_to_complex(float2 *grid, const int width);

	__global__ void fft_shift_complex_to_real(float2 *grid, float *image, const int width);
#endif


#ifdef __cplusplus
}
#endif


#endif
