#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd $SCRIPT_DIR

if [ -z $SCHEDULER_MODES ]; then
  SCHEDULER_MODES="virtual-fifo"
  # SCHEDULER_MODES="virtual-fifo ring-buffer"
fi

ORDERS="2 3 4 5 6 8 10 13 16 21 26 32 42 53 64 85 106 128 170 213 256 341 426 512 682"

# Run timing measurements sequentially (to avoid interference)
echo "Running timing measurements sequentially..."
for MATRIX_ORDER in $ORDERS; do
    export MATRIX_ORDER=$MATRIX_ORDER
    for SCHEDULER_MODE in $SCHEDULER_MODES; do
      export PARAMETERS="${MATRIX_ORDER}x${MATRIX_ORDER}"
      export SCHEDULER_MODE=$SCHEDULER_MODE
      export INSTANCE_DIR=$SCRIPT_DIR/instances/$SCHEDULER_MODE/$PARAMETERS
      make clean
      make -j all

      echo "Running timing profile for $SCHEDULER_MODE $PARAMETERS..."
      sh profile.sh
      if [[ $? != 0 ]]; then
        echo failed
        exit 1
      fi
    done
  done
done
echo "All timing profiles completed."
