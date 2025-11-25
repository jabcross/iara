# Regression test: 8192x8192 matrix, 8 blocks, vf-omp scheduler
# Fixed configuration

NUM_BLOCKS=8
MATRIX_SIZE=8192

# IaRa configuration
IARA_OPT_SCHEDULER="virtual-fifo"
IARA_RUNTIME_BACKEND="omp"

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE -DSCHEDULER_IARA -DVERBOSE"
export EXTRA_LINKER_ARGS='-lopenblas'
