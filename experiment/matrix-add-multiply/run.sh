#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd $SCRIPT_DIR

mkdir -p profile

rm -f time_out.txt time_err.txt

for i in {2..10}; do
  export MATRIX_ORDER=$((2 ** $i))
  SCHEDULER_MODE=virtual-fifo sh -x ./build.sh
  \time -v build/a.out >profile/time_out_${MATRIX_ORDER}x${MATRIX_ORDER}.txt 2>>time_err_${MATRIX_ORDER}x${MATRIX_ORDER}.txt
done
