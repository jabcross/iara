# Regression test: 10080x10080 matrix, 10 blocks, omp-for scheduler
# Fixed configuration

NUM_BLOCKS=10
MATRIX_SIZE=10080

# Baseline scheduler - no IaRa
# IARA_OPT_SCHEDULER and IARA_RUNTIME_BACKEND are not needed

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE -DSCHEDULER_OMP_FOR -DVERBOSE"
export EXTRA_LINKER_ARGS='-lopenblas'
