# export EXTRA_KERNEL_ARGS='-DNUM_BLOCKS=4 -DMATRIX_SIZE=8192 -DVERBOSE'


if [[ -z $NUM_BLOCKS ]]; then
  echo "Must specify NUM_BLOCKS"
  exit 1
fi

if [[ -z $MATRIX_SIZE ]]; then
  echo "Must specify MATRIX_SIZE"
  exit 1
fi

# Set scheduler compile flag based on SCHEDULER_MODE
SCHEDULER_FLAG=""
if [[ "$SCHEDULER_MODE" == "sequential" ]]; then
  SCHEDULER_FLAG="-DSCHEDULER_SEQUENTIAL"
elif [[ "$SCHEDULER_MODE" == "omp-for" ]]; then
  SCHEDULER_FLAG="-DSCHEDULER_OMP_FOR"
elif [[ "$SCHEDULER_MODE" == "omp-task" ]]; then
  SCHEDULER_FLAG="-DSCHEDULER_OMP_TASK"
elif [[ "$SCHEDULER_MODE" == "virtual-fifo" ]] || [[ "$SCHEDULER_MODE" == "ring-buffer" ]]; then
  SCHEDULER_FLAG="-DSCHEDULER_IARA"
else
  echo "Unknown SCHEDULER_MODE: $SCHEDULER_MODE"
  exit 1
fi

export EXTRA_KERNEL_ARGS="-DNUM_BLOCKS=$NUM_BLOCKS -DMATRIX_SIZE=$MATRIX_SIZE $SCHEDULER_FLAG -DVERBOSE"

export EXTRA_LINKER_ARGS='-lopenblas'
# export EXTRA_RUNTIME_ARGS='-DIARA_DISABLE_OMP -DIARA_DEBUGPRINT'
# export EXTRA_RUNTIME_ARGS='-DIARA_DISABLE_OMP'
# export EXTRA_RUNTIME_ARGS='-DIARA_DEBU'
