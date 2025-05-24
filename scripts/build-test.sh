#!/bin/bash

# Check if exactly two arguments were provided
if [ "$#" -ne 2 ]; then
  echo "Error: Exactly two arguments are required."
  echo "Usage: $0 <path/to/test> <scheduler-mode>"
  exit 1
fi

# Store arguments in variables
PATH_TO_TEST="$1"
SCHEDULER_MODE="$2"

cd $PATH_TO_TEST

FOLDER_NAME=$(basename $(pwd))

mkdir -p build
cd build

rm *.o

echo "Building test: $FOLDER_NAME"
echo ""

if [ "$SCHEDULER_MODE" == "--dynamic-push-first-fifo-scheduler" ]; then
  SCHEDULER_SOURCES="-c $IARA_DIR/runtime/DynamicPushFirstFIFOScheduler.cpp \
           -c $IARA_DIR/runtime/DynamicPushFirstFIFO.cpp"
else
  echo "No recognized scheduler mode".
  exit 1
fi

INCLUDES="-I. -I$IARA_DIR/include -I$LLVM_DIR/mlir/include -I$LLVM_DIR/llvm/include -I$LLVM_DIR/build/lib/clang/*/include"

iara-opt --flatten $SCHEDULER_MODE ../topology.test >schedule.mlir
mlir-to-llvmir.sh schedule.mlir

shopt -s nullglob

echo building schedule
clang++ -g -xir -c schedule.ll -o schedule.o

ls ..

echo building c kernels
for c_file in ../*.c; do
  echo "Compiling $c_file"
  clang++ -g -xc -c "$c_file" $INCLUDES
done

echo building cpp kernels
for cpp_file in ../*.cpp; do
  echo "Compiling $cpp_file"
  clang++ -g -xc++ -std=c++20 -c "$cpp_file" $INCLUDES
done

echo building runtime
clang++ -g -xc++ -std=c++20 -fopenmp=libomp $SCHEDULER_SOURCES $INCLUDES

echo linking
clang++ -g -lomp -lpthread -fuse-ld=mold *.o $INCLUDES
