if [ -z "${IARA_OPT_SCHEDULER:-}" ] || [ -z "${IARA_RUNTIME_BACKEND:-}" ]; then
  echo "Error: environment variables IARA_OPT_SCHEDULER and IARA_RUNTIME_BACKEND must be defined" >&2
  exit 1
fi

# Cholesky test with configurable parameters
NUM_BLOCKS=2
MATRIX_SIZE=2048

# IaRa configuration (this test uses virtual-fifo scheduler with OpenMP backend)

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE -DSCHEDULER_IARA -DVERBOSE"
export EXTRA_LINKER_ARGS='-lopenblas'
