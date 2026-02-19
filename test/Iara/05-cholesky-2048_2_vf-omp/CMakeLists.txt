# Build configuration from experiments.yaml
set(EXTRA_KERNEL_ARGS -DVERBOSE;-DMATRIX_SIZE=2048;-DNUM_BLOCKS=2;-DBLOCK_SIZE=1024;-DSCHEDULER_VF_OMP CACHE STRING "Extra kernel compilation arguments")
set(EXTRA_LINKER_ARGS -lopenblas CACHE STRING "Extra linker arguments")

# Export parameters as environment variables for codegen.sh
set(ENV{MATRIX_SIZE} 2048)
set(ENV{NUM_BLOCKS} 2)
set(ENV{BLOCK_SIZE} 1024)
