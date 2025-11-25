# Regression test: 10080x10080 matrix, 10 blocks, vf-omp scheduler
# Fixed configuration

NUM_BLOCKS=10
MATRIX_SIZE=10080

# IaRa configuration
IARA_OPT_SCHEDULER="virtual-fifo"
IARA_RUNTIME_BACKEND="omp"

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE -DSCHEDULER_IARA -DVERBOSE"
export EXTRA_LINKER_ARGS='-lopenblas'
