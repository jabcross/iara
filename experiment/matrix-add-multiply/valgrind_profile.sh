#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd $SCRIPT_DIR

if [ -z $SCHEDULER_MODES ]; then
  SCHEDULER_MODES="virtual-fifo"
  # SCHEDULER_MODES="virtual-fifo ring-buffer"
fi

# Store background job PIDs for valgrind processes
declare -a valgrind_pids=()

ORDERS="2 3 4 5 6 8 10 13 16 21 26 32 42 53 64 85 106 128 170 213 256 341 426 512 682"

# Run valgrind measurements in parallel (they don't interfere with each other)
echo "Running valgrind measurements in parallel..."
for MATRIX_ORDER in $ORDERS; do
  for SCHEDULER_MODE in $SCHEDULER_MODES; do
    export MATRIX_ORDER=$MATRIX_ORDER
    export PARAMETERS="${MATRIX_ORDER}x${MATRIX_ORDER}"
    export SCHEDULER_MODE=$SCHEDULER_MODE
    export INSTANCE_DIR=$SCRIPT_DIR/instances/$SCHEDULER_MODE/$PARAMETERS

    # Run the valgrind profiling in parallel (background process)
    (
      mkdir -p $INSTANCE_DIR/profile
      cd $INSTANCE_DIR/profile

      echo "Running valgrind memory profiling for $INSTANCE_DIR..."

      # Run valgrind massif profiling
      valgrind --tool=massif --massif-out-file=massif.out $INSTANCE_DIR/build/a.out

      echo "Valgrind profiling completed for $INSTANCE_DIR"
    ) &

    valgrind_pids+=($!)
  done
done

echo "Waiting for valgrind profiles to complete..."
for pid in "${valgrind_pids[@]}"; do
  wait $pid
done
echo "All valgrind profiles completed."
