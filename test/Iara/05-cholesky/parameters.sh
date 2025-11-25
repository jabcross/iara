# Cholesky test with configurable parameters
# NUM_BLOCKS and MATRIX_SIZE can come from environment or use defaults

if [[ -z $NUM_BLOCKS ]]; then
  NUM_BLOCKS=4
fi

if [[ -z $MATRIX_SIZE ]]; then
  MATRIX_SIZE=2048
fi

# IaRa configuration (this test uses virtual-fifo scheduler with OpenMP backend)
IARA_OPT_SCHEDULER="virtual-fifo"
IARA_RUNTIME_BACKEND="omp"

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE -DSCHEDULER_IARA -DVERBOSE"
export EXTRA_LINKER_ARGS='-lopenblas'
