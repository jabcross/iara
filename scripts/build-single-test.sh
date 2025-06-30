#!/bin/bash

set -x

echo $PATH

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

$IARA_DIR/scripts/build-iara.sh

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

if [ ! -d $IARA_DIR/runtime/$SCHEDULER_MODE ]; then
  echo "Invalid scheduler mode."
  exit 1
fi

echo SCHEDULER_MODE = \"$SCHEDULER_MODE\"

SCHEDULER_SOURCES="-c $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* "

echo SCHEDULER_SOURCES = \"$SCHEDULER_SOURCES\"

# if [ "$SCHEDULER_MODE" == "virtual-fifo" ]; then
#   SCHEDULER_SOURCES="-c $IARA_DIR/runtime/SDF_OoO*.cpp"
# else
#   echo "No recognized scheduler mode: --$SCHEDULER_MODE".
#   exit 1
# fi

OMP_INCLUDE=/usr/lib/clang/19/include

INCLUDES="$INCLUDES -I. -I$IARA_DIR/include -I$IARA_DIR/external -I$LLVM_DIR/mlir/include -I$LLVM_DIR/llvm/include -I$OMP_INCLUDE"

if [[ $IARA_FLAGS == "" ]]; then
  IARA_FLAGS="--flatten --$SCHEDULER_MODE='main-actor=run'"
fi

COMPILER_FLAGS='-stdlib=libc++ -fopenmp '
LINKER_FLAGS='-lomp -lpthread -lc++ -lc++abi'

source $PATH_TO_TEST_SOURCES/extra_args.sh

pwd
pwd >&2

\time -f 'iara-opt took %E and returned code %x' bash -xc "iara-opt $IARA_FLAGS $PATH_TO_TEST_SOURCES/topology.test >schedule.mlir 2>/dev/null"

sh -x mlir-to-llvmir.sh schedule.mlir

shopt -s nullglob

echo building schedule
\time -f 'compiling schedule took %E and returned code %x' bash -xc "ccache clang++ --std=c++20 -g $COMPILER_FLAGS $INCLUDES -xir -c schedule.ll -o schedule.o"
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
    \time -f 'compiling c kernels took %E and returned code %x' bash -xc "ccache clang++  -g $COMPILER_FLAGS $INCLUDES $EXTRA_KERNEL_ARGS -xc -c "$c_file" "
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
    \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "ccache clang -g $INCLUDES -xc++ -std=c++20 $COMPILER_FLAGS $INCLUDES -c $cpp_file $SCHEDULER_SOURCES "
    RC=$?
    echo cpp kernels return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build cpp files"
      exit 3
    fi
  done
fi

echo building runtime
\time -f 'compiling runtime took %E and returned code %x' bash -xc "ccache clang++ -ftime-trace --std=c++20 -g $COMPILER_FLAGS $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
# \time -f 'compiling runtime took %E and returned code %x' bash -xc "clang++ --std=c++20 -g -xc++ -std=c++20  $SCHEDULER_SOURCES $INCLUDES"
RC=$?
echo executable return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to build runtime"
  exit 4
fi

EXTRAOBJS=""

for DIR in $EXTRA_OBJ_DIRS; do
  EXTRAOBJS="$EXTRAOBJS $PATH_TO_TEST_SOURCES/$DIR/*.o"
done

echo linking
\time -f 'linking took %E and returned code %x' bash -xc "ccache clang++ --std=c++20 -g -fuse-ld=mold $LINKER_FLAGS $INCLUDES $EXTRA_LINKER_ARGS *.o $EXTRAOBJS"
RC=$?
echo linker return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to link"
  exit 5
fi
