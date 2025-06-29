#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd $SCRIPT_DIR

if [ -z $SCHEDULER_MODE ]; then
  echo "Must specify SCHEDULER_MODE"
  exit 1
fi

mkdir -p profile

rm -f profile/*

for i in {2..10}; do
  export MATRIX_ORDER=$((2 ** $i))
  export EXPERIMENT_SUFFIX="${MATRIX_ORDER}x${MATRIX_ORDER}.$SCHEDULER_MODE"
  SCHEDULER_MODE=$SCHEDULER_MODE EXPERIMENT_SUFFIX=$EXPERIMENT_SUFFIX sh -x ./build.sh
  \time -v build/$EXPERIMENT_SUFFIX.bin >profile/time_out_$EXPERIMENT_SUFFIX.txt 2>profile/time_err_$EXPERIMENT_SUFFIX.txt
done
