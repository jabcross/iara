#!/bin/bash

cd build

DISABLE_RAISE=True
DISABLE_CLANG=True

. ../../build.sh

$CLANG_BIN ../src/build_dog_pyr.c ../src/build_gaussian_pyramid.c ../src/build_grd_rot_pyr.c ../src/detect_keypoint.c ../src/extract_descriptor.c ../src/ezsift-common.c ../src/filenames.c ../src/img.c ../src/io.c ../src/main.c ../src/match_keypoints.c ../src/ordered_chained_list.c -I../include -I../include/x86 schedule.ll -o main -lm

if ! ./main ; then
  export ERRORMSG="executable failed"
  echo $ERRORMSG | tee errormsg.txt 1>&2
  exit 1
fi
