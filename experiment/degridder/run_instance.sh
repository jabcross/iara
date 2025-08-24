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

OMP_NUM_THREADS=$NUM_CORES OMP_PROC_BIND=true OMP_PLACES=cores /usr/bin/time -v -o ../preesm_degridder_time.txt taskset -c $CPU_MASK ./degridder_pipeline >../preesm_degridder_stdout.txt 2>../preesm_degridder_stderr.txt

cd ../build

OMP_NUM_THREADS=$NUM_CORES OMP_PROC_BIND=true OMP_PLACES=cores /usr/bin/time -v -o ../iara_degridder_time.txt taskset -c $CPU_MASK ./degridder_pipeline >../iara_degridder_stdout.txt 2>../iara_degridder_stderr.txt
