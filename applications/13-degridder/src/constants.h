#ifndef CONSTANTS_H
#define CONSTANTS_H

// Auto-generated constants from scenario parameters
// When compiling, provide these as -DNAME=value (e.g., -DGRID_SIZE=1024)

// Provide defaults if not defined at compile time (for local development builds)
#ifndef GRID_SIZE
#define GRID_SIZE 1024
#endif

#ifndef NUM_KERNEL_SUPPORT
#define NUM_KERNEL_SUPPORT 8
#endif

#ifndef OVERSAMPLING_FACTOR
#define OVERSAMPLING_FACTOR 3
#endif

#ifndef NUMBER_SAMPLE_IN_KERNEL
#define NUMBER_SAMPLE_IN_KERNEL 65
#endif

#ifndef NUM_KERNELS
#define NUM_KERNELS 513
#endif

#ifndef NUM_SCENARIO
#define NUM_SCENARIO 1
#endif

#ifndef NUM_VISIBILITIES
#define NUM_VISIBILITIES 2097152
#endif

#ifndef TOTAL_KERNELS_SAMPLES
#define TOTAL_KERNELS_SAMPLES 2162881
#endif

#ifndef NUM_CHUNK
#define NUM_CHUNK 1
#endif

#ifndef NUM_VISIB_D_N_CHUNK
#define NUM_VISIB_D_N_CHUNK (NUM_VISIBILITIES / NUM_CHUNK)
#endif

#endif // CONSTANTS_H
