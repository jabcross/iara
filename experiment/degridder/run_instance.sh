#!/bin/bash

pwd

if [[ $(basename $(realpath .)) -ne "build" ]]; then
  echo "Run this in instance build directory."
  exit 1
fi

if [[ ! -f "degridder_pipeline" ]]; then
  echo "degridder_pipeline missing".
  exit 1
fi

cd ..

rm -f data
rm -f output
ln -s ../../Code/data .
ln -s ../../Code/output .

cd build

./degridder_pipeline

cd ../preesm_build

./degridder_pipeline
