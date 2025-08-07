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

mkdir -p $PATH_TO_TEST_BUILD_DIR

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

if [ -z $LLVM_INSTALL ]; then
  echo "Must provide LLVM_INSTALL"
  exit 1
fi

if [ -f $PATH_TO_TEST_SOURCES/test-setup.sh ]; then
  source $PATH_TO_TEST_SOURCES/test-setup.sh
fi

if [ -z $TOPOLOGY_FILE ]; then
  TOPOLOGY_FILE="$PATH_TO_TEST_SOURCES/topology.test"
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

CLANG_INCLUDE="$LLVM_INSTALL/lib/clang/21/include"

INCLUDES="-I$CLANG_INCLUDE -I. -I$IARA_DIR/include -I$IARA_DIR/external"

if [[ $IARA_FLAGS == "" ]]; then
  IARA_FLAGS="--iara-canonicalize --flatten --$SCHEDULER_MODE='main-actor=run'"
fi

CPP_COMPILER=$LLVM_INSTALL/bin/clang++
C_COMPILER=$LLVM_INSTALL/bin/clang
COMPILER_FLAGS="-stdlib=libc++ -fopenmp"
LINKER_FLAGS="-L$LLVM_INSTALL/lib -lomp -lpthread -lc++ -lc++abi "

# EXTRA_RUNTIME_FLAGS=-DIARA_DEBUGPRINT

if [ -f "$PATH_TO_TEST_SOURCES/extra_args.sh" ]; then
  source "$PATH_TO_TEST_SOURCES/extra_args.sh"
fi

pwd
pwd >&2

\time -f 'iara-opt took %E and returned code %x' bash -xc "iara-opt $IARA_FLAGS $TOPOLOGY_FILE >schedule.mlir 2>/dev/null"
RC=$?
echo iara-opt return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to run iara-opt"
  exit 1
fi

sh -x mlir-to-llvmir.sh schedule.mlir
RC=$?
echo mlir-to-llvmir return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to convert to llvm ir"
  exit 1
fi

shopt -s nullglob

echo building schedule
\time -f 'compiling schedule took %E and returned code %x' bash -xc "$CPP_COMPILER -ftime-trace=schedule.json --std=c++20 -g $COMPILER_FLAGS $INCLUDES -xir -c schedule.ll -o schedule.o"
RC=$?
echo scheduler return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to build schedule"
  exit 1
fi

ls ..

pwd >&2

echo building c kernels
if [ "$(ls $PATH_TO_TEST_SOURCES/*.c 2>/dev/null)" ]; then
  for c_file in $PATH_TO_TEST_SOURCES/*.c; do
    echo "Compiling $c_file"
    \time -f 'compiling c kernels took %E and returned code %x' bash -xc "$CPP_COMPILER -g $COMPILER_FLAGS -ftime-trace=ckernels.json $INCLUDES $EXTRA_KERNEL_ARGS -xc -c "$c_file" "
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
    \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "$CPP_COMPILER -g $INCLUDES -xc++ -std=c++20 -ftime-trace=cppkernels.json $COMPILER_FLAGS $INCLUDES -c $cpp_file $SCHEDULER_SOURCES "
    RC=$?
    echo cpp kernels return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build cpp files"
      exit 3
    fi
  done
fi

echo building runtime
\time -f 'compiling runtime took %E and returned code %x' bash -xc "$CPP_COMPILER -ftime-trace=runtime.json --std=c++20 -g $COMPILER_FLAGS $EXTRA_RUNTIME_FLAGS $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
# \time -f 'compiling runtime took %E and returned code %x' bash -xc "$CPP_COMPILER --std=c++20 -g -xc++ -std=c++20  $SCHEDULER_SOURCES $INCLUDES"
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
\time -f 'linking took %E and returned code %x' bash -xc "$CPP_COMPILER --std=c++20 -g -fuse-ld=mold $LINKER_FLAGS $INCLUDES $EXTRA_LINKER_ARGS *.o $EXTRAOBJS"
RC=$?
echo linker return code: $?
if [ $RC -ne 0 ]; then
  echo "Error: Failed to link"
  exit 5
fi
