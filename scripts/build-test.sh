#!/bin/bash

set -x

# Check if exactly two arguments were provided
if [ "$#" -lt 1 ]; then
  echo "Error: At least one argument is required."
  echo "Usage: $0 <path/to/test> [scheduler-mode]"
  exit 1
fi

# Store arguments in variables
PATH_TO_TEST="$1"

cd $PATH_TO_TEST

FOLDER_NAME=$(basename $(pwd))

mkdir -p build
cd build

rm *.o

echo "Building test: $FOLDER_NAME"
echo ""

if [[ $2 != "" ]]; then
  SCHEDULER_MODE=$2
fi

SCHEDULER_SOURCES="-c $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* "

# if [ "$SCHEDULER_MODE" == "ooo-scheduler" ]; then
#   SCHEDULER_SOURCES="-c $IARA_DIR/runtime/SDF_OoO*.cpp"
# else
#   echo "No recognized scheduler mode: --$SCHEDULER_MODE".
#   exit 1
# fi

INCLUDES="-I. -I$IARA_DIR/include -I$IARA_DIR/external -I$LLVM_DIR/mlir/include -I$LLVM_DIR/llvm/include -I$LLVM_DIR/build/lib/clang/*/include"

iara-opt --flatten --$SCHEDULER_MODE ../topology.test >schedule.mlir

mlir-to-llvmir.sh schedule.mlir

shopt -s nullglob

echo building schedule
clang++ -g -xir -c schedule.ll -o schedule.o
echo scheduler return code: $?

ls ..

echo building c kernels
for c_file in ../*.c; do
  echo "Compiling $c_file"
  clang++ -g -xc -c "$c_file" $INCLUDES
  echo c kernels return code: $?
done

echo building cpp kernels
for cpp_file in ../*.cpp; do
  echo "Compiling $cpp_file"
  clang++ -g -xc++ -std=c++20 -c "$cpp_file" $INCLUDES
  echo cpp kernels return code: $?
done

echo building runtime
clang++ -g -xc++ -std=c++20 -fopenmp=libomp $SCHEDULER_SOURCES $INCLUDES
echo executable return code: $?

echo linking
clang++ -g -lomp -lpthread -fuse-ld=mold *.o $INCLUDES
echo linker return code: $?
