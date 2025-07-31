#!/bin/bash

SOURCE_DIR="/home/jabcross/repos/degridder/Code"

INSTANCE_NAME=$(basename $(realpath ..))
MAIN_ACTOR_NAME=$(python -c "print('top_parallel_degridder_complete' if 'complete' in '$INSTANCE_NAME' else 'top_parallel_degridder')")

iara-opt --iara-canonicalize --flatten --virtual-fifo=main-actor=$MAIN_ACTOR_NAME "topology.mlir" > "schedule.mlir"

$LLVM_INSTALL/bin/clang -g -fopenmp -xir -c schedule.ll -o schedule.o

$LLVM_INSTALL/bin/clang++ -std=c++26 -g -fopenmp -c -I$SOURCE_DIR/include main.cpp -o broadcasts.o

OBJS=$(for i in fft_run degridding common top vis_to_csv; do echo -n "/home/jabcross/repos/degridder/Code/build/CMakeFiles/degridder_pipeline.dir/src/$i.c.o " ; done )

$LLVM_INSTALL/bin/clang++ -stdlib=libc++ -L$LLVM_INSTALL/lib -lomp -lpthread -lc++ -lc++abi -L/usr/lib64 -L/usr/local/lib -lfftw3 *.o $OBJS
