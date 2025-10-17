#!/bin/bash

pwd

if [[ $(basename $(realpath .)) -ne "build" ]]; then
  echo "Run this in instance build directory."
  exit 1
fi

if [[ -z "$NUM_CORES" ]]; then
  echo "NUM_CORES not set. Please set it before running this script."
  exit 1
fi

if [[ "$NUM_CORES" == 1 ]]; then
  CPU_MASK=0
elif [[ "$NUM_CORES" == 2 ]]; then
  CPU_MASK=0,1
else
  CPU_MASK=0-$(($NUM_CORES - 1))
fi

if [[ ! -f "degridder_pipeline" ]]; then
  echo "degridder_pipeline missing".
  exit 1
fi

cd ../

rm -f data
rm -f output
ln -s ../../Code/data .
ln -s ../../Code/output .

cd preesm_build

echo Instance $ITER run at $(date) >> >(tee -a measurements/degridder_stdout.txt)
echo Instance $ITER run at $(date) >> >(tee -a measurements/degridder_stderr.txt)

cp build/degridder_pipeline .

OMP_NUM_THREADS=$NUM_CORES OMP_PROC_BIND=true OMP_PLACES=cores /usr/bin/time -v -o >(tee -a measurements/degridder_time_$ITER.txt) taskset -c $CPU_MASK ./degridder_pipeline >> >(tee -a measurements/degridder_stdout.txt) 2>> >(tee -a measurements/degridder_stderr.txt)

cd ../build

echo Instance $ITER run at $(date) >> >(tee -a measurements/degridder_stdout.txt)
echo Instance $ITER run at $(date) >> >(tee -a measurements/degridder_stderr.txt)

OMP_NUM_THREADS=$NUM_CORES OMP_PROC_BIND=true OMP_PLACES=cores /usr/bin/time -v -o >(tee -a measurements/degridder_time_$ITER.txt) taskset -c $CPU_MASK ./degridder_pipeline >> >(tee -a measurements/degridder_stdout.txt) 2>> >(tee -a measurements/degridder_stderr.txt)
