#!/bin/bash

set -x

# Check if exactly two arguments were provided
if [ "$#" -lt 1 ]; then
  echo "Error: At least one argument is required."
  echo "Usage: $0 <path/to/test> [scheduler-mode]"
  exit 1
fi

# Store arguments in variables

FOLDER_NAME=$(basename $(realpath $1))

PATH_TO_TEST_SOURCES=$IARA_DIR/test/Iara/$FOLDER_NAME
PATH_TO_TEST_BUILD_DIR=$IARA_DIR/build/test/Iara/$FOLDER_NAME

$IARA_DIR/scripts/build.sh

cd $PATH_TO_TEST_BUILD_DIR

if [[ $(basename $(realpath ..)) != "Iara" ]]; then
  echo "Wrong directory!"
  exit 1
fi

mkdir -p build
cd build

rm -rf *

rm *.o

echo "Building test: $FOLDER_NAME"
echo ""

if [[ $2 != "" ]]; then
  SCHEDULER_MODE=$2
fi

echo SCHEDULER_MODE = \"$SCHEDULER_MODE\"

SCHEDULER_SOURCES="-c $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* "

echo SCHEDULER_SOURCES = \"$SCHEDULER_SOURCES\"

# if [ "$SCHEDULER_MODE" == "ooo-scheduler" ]; then
#   SCHEDULER_SOURCES="-c $IARA_DIR/runtime/SDF_OoO*.cpp"
# else
#   echo "No recognized scheduler mode: --$SCHEDULER_MODE".
#   exit 1
# fi

INCLUDES="$INCLUDES -I. -I$IARA_DIR/include -I$IARA_DIR/external -I$LLVM_DIR/mlir/include -I$LLVM_DIR/llvm/include -I$LLVM_DIR/build/lib/clang/*/include -I$LLVM_DIR/ -fopenmp=libomp -lomp -lpthread"

source $PATH_TO_TEST_SOURCES/extra_args.sh

pwd
pwd >&2

\time -f 'iara-opt took %E and returned code %x' bash -xc "iara-opt --flatten --$SCHEDULER_MODE $PATH_TO_TEST_SOURCES/topology.test >schedule.mlir 2>/dev/null"

sh -x mlir-to-llvmir.sh schedule.mlir

shopt -s nullglob

echo building schedule
\time -f 'compiling schedule took %E and returned code %x' bash -xc "ccache clang++ -g $INCLUDES -xir -c schedule.ll -o schedule.o"
RC
echo scheduler return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to build schedule"b
  exit 1
fi

ls ..

pwd >&2

echo building c kernels
if [ "$(ls $PATH_TO_TEST_SOURCES/*.c 2>/dev/null)" ]; then
  for c_file in $PATH_TO_TEST_SOURCES/*.c; do
    echo "Compiling $c_file"
    \time -f 'compiling c kernels took %E and returned code %x' bash -xc "ccache clang -g  $INCLUDES $EXTRA_KERNEL_ARGS -xc -c "$c_file" "
    RC=$?
    echo c kernels return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build c files"
      exit 2
    fi
  done
fi

echo building cpp kernels
if [ "$(ls $PATH_TO_TEST_SOURCES/*.cpp 2>/dev/null)" ]; then
  for cpp_file in $PATH_TO_TEST_SOURCES/*.cpp; do
    echo "Compiling $cpp_file"
    \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "ccache clang -g $INCLUDES -xc++ -std=c++20 $INCLUDES -c "$cpp_file""
    RC=$?
    echo cpp kernels return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build cpp files"
      exit 3
    fi
  done
fi

echo building runtime
\time -f 'compiling runtime took %E and returned code %x' bash -xc "ccache clang++ -g $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
# \time -f 'compiling runtime took %E and returned code %x' bash -xc "clang++ -g -xc++ -std=c++20  $SCHEDULER_SOURCES $INCLUDES"
RC=$?
echo executable return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to build runtime"
  exit 4
fi

echo linking
\time -f 'linking took %E and returned code %x' bash -xc "ccache clang++ -fopenmp=libomp -g -fuse-ld=mold $INCLUDES $EXTRA_LINKER_ARGS *.o"
RC=$?
echo linker return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to link"
  exit 5
fi
