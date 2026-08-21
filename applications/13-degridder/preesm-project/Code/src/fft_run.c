
#include "fft_run.h"
#include <fftw3.h>

void CUFFT_EXECUTE_INVERSE_C2C_actor(int GRID_SIZE, float2 *uv_grid_in, float2 *uv_grid_out){
	fftwf_plan fft_plan;

	fft_plan = fftwf_plan_dft_2d(GRID_SIZE, GRID_SIZE, (float (*)[2]) uv_grid_in, (float (*)[2]) uv_grid_out, FFTW_BACKWARD, FFTW_ESTIMATE);
	fftwf_execute(fft_plan);
	fftwf_destroy_plan(fft_plan);

	//Normalising after FFT
	for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
		uv_grid_out[i].x /= GRID_SIZE;
		uv_grid_out[i].y /= GRID_SIZE;
	}

	//printf("CUFFT_EXECUTE_INVERSE_C2C_actor\n");


}

void fft_shift_complex_to_complex_actor(int GRID_SIZE, float2 *uv_grid_in, float2 *uv_grid_out) {
	int row_index,col_index;

	// Perform 2D FFT shift
	for (row_index = 0;row_index < GRID_SIZE; row_index++)
	{
		for (col_index = 0; col_index < GRID_SIZE; col_index++)
		{
			int a = 1 - 2 * ((row_index + col_index) & 1);
			uv_grid_out[row_index * GRID_SIZE + col_index].x = uv_grid_in[row_index * GRID_SIZE + col_index].x * a;
			uv_grid_out[row_index * GRID_SIZE + col_index].y = uv_grid_in[row_index * GRID_SIZE + col_index].y * a;
		}
	}
	//printf("fft_shift_complex_to_complex_actor\n");
}

void CUFFT_EXECUTE_FORWARD_C2C_actor(int GRID_SIZE, float2 *uv_grid_in, float2 *uv_grid_out){

		fftwf_plan fft_plan;

		fft_plan = fftwf_plan_dft_2d(GRID_SIZE, GRID_SIZE, (float (*)[2]) uv_grid_in, (float (*)[2]) uv_grid_out, FFTW_FORWARD, FFTW_ESTIMATE);
		fftwf_execute(fft_plan);
		fftwf_destroy_plan(fft_plan);

		// Normalising after the FFT
		for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
			uv_grid_out[i].x /= GRID_SIZE;
			uv_grid_out[i].y /= GRID_SIZE;
		}
		//printf("CUFFT_EXECUTE_FORWARD_C2C_actor\n");
}


void fft_shift_complex_to_real_actor(int GRID_SIZE, float2 *uv_grid, float * dirty_image) {
    int row_index, col_index;

    // Perform 2D FFT shift back
    for (row_index = 0; row_index < GRID_SIZE; row_index++) {
        for (col_index = 0; col_index < GRID_SIZE; col_index++) {
            int a = 1 - 2 * ((row_index + col_index) & 1);
            dirty_image[row_index * GRID_SIZE + col_index] = uv_grid[row_index * GRID_SIZE + col_index].x * a;
        }
    }
    //printf("fft_shift_complex_to_real_actor\n");


}


void fft_shift_real_to_complex_actor(int GRID_SIZE, float *input_grid, Config *config, float2 *fourier) {
	int row_index,col_index;

	// Perform 2D FFT shift back
	for (row_index = 0; row_index < GRID_SIZE; row_index++)
	{
		for (col_index = 0; col_index < GRID_SIZE; col_index++)
		{
			int a = 1 - 2 * ((row_index + col_index) & 1);
			fourier[row_index * GRID_SIZE + col_index].x = input_grid[row_index * GRID_SIZE + col_index] * a;
			fourier[row_index * GRID_SIZE + col_index].y = 0;

		}
	}
	//printf("fft_shift_real_to_complex_actor\n");
}

