#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

if [[ $MATRIX_ORDER == "" ]]; then
  echo "must pass in MATRIX_ORDER"
  exit 1
fi

if [[ $EXPERIMENT_SUFFIX == "" ]]; then
  echo "must pass in EXPERIMENT_SUFFIX"
  exit 1
fi

MATRIXNUMELEMENTS=$(($MATRIX_ORDER * $MATRIX_ORDER))

echo "Matrix size: $MATRIXNUMELEMENTS"

cd $SCRIPT_DIR

mkdir -p build
rm -f build/*

cd build

sed -e "s/MATRIXNUMELEMENTS/$MATRIXNUMELEMENTS/g" ../topology.mlir.template >topology.$EXPERIMENT_SUFFIX.mlir

# rebuild compiler
$IARA_DIR/scripts/build-iara.sh

PATH_TO_TEST_SOURCES=$(realpath ..)
PATH_TO_TEST_BUILD_DIR=$(realpath .)

if [[ -z "$SCHEDULER_MODE" ]] || [[ ! -d ${IARA_DIR}/runtime/${SCHEDULER_MODE} ]]; then
  echo "Invalid SCHEDULER_MODE: \`$SCHEDULER_MODE\`"
  exit 1
fi

echo SCHEDULER_MODE = \"$SCHEDULER_MODE\"

SCHEDULER_SOURCES="-c $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* "

echo SCHEDULER_SOURCES = \"$SCHEDULER_SOURCES\"

OMP_INCLUDE=/usr/lib/clang/19/include

INCLUDES="$INCLUDES -I. -I$IARA_DIR/include -I$IARA_DIR/external -I$LLVM_DIR/mlir/include -I$LLVM_DIR/llvm/include -I$OMP_INCLUDE"

if [[ $IARA_FLAGS == "" ]]; then
  IARA_FLAGS="--flatten --$SCHEDULER_MODE='main-actor=run'"
fi

COMPILER_FLAGS="-stdlib=libc++ -fopenmp -DMATRIXORDER=$MATRIX_ORDER"
LINKER_FLAGS='-lomp -lpthread -lc++ -lc++abi'

# COMPILER_FLAGS="-stdlib=libc++ -DMATRIXORDER=$MATRIX_ORDER"
# LINKER_FLAGS=' -lc++ -lc++abi'

if [[ -f $PATH_TO_TEST_SOURCES/extra_args.sh ]]; then
  source $PATH_TO_TEST_SOURCES/extra_args.sh
fi

pwd
pwd >&2

\time -f 'iara-opt took %E and returned code %x' bash -xc "iara-opt $IARA_FLAGS $PATH_TO_TEST_BUILD_DIR/topology.$EXPERIMENT_SUFFIX.mlir >schedule.$EXPERIMENT_SUFFIX.mlir 2>/dev/null"
RC=$?
echo iara-opt return code: $RC
if [ $RC -ne 0 ]; then
  echo "Error: Failed to run iara"
  exit 1
fi

sh -x mlir-to-llvmir.sh schedule.$EXPERIMENT_SUFFIX.mlir

shopt -s nullglob

echo building schedule
\time -f 'compiling schedule took %E and returned code %x' bash -xc "ccache clang++ --std=c++17 -g $COMPILER_FLAGS $INCLUDES -xir -c schedule.$EXPERIMENT_SUFFIX.ll -o schedule.o"
RC=$?
echo scheduler return code: $RC
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
    \time -f 'compiling c kernels took %E and returned code %x' bash -xc "ccache clang++  -g $COMPILER_FLAGS $INCLUDES $EXTRA_KERNEL_ARGS -xc -c "$c_file" "
    RC=$?
    echo c kernels return code: $RC
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
    \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "ccache clang -v -g $INCLUDES -xc++ -std=c++20 $COMPILER_FLAGS $INCLUDES -c "$cpp_file""
    RC=$?
    echo cpp kernels return code: $RC
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build cpp files"
      exit 3
    fi
  done
fi

echo building runtime
\time -f 'compiling runtime took %E and returned code %x' bash -xc "ccache clang++ -ftime-trace --std=c++17 -g $COMPILER_FLAGS $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
# \time -f 'compiling runtime took %E and returned code %x' bash -xc "clang++ --std=c++17 -g -xc++ -std=c++20  $SCHEDULER_SOURCES $INCLUDES"
RC=$?
echo executable return code: $RC
if [ $RC -ne 0 ]; then
  echo "Error: Failed to build runtime"
  exit 4
fi

echo linking
\time -f 'linking took %E and returned code %x' bash -xc "ccache clang++ -v --std=c++17 -g -fuse-ld=mold $LINKER_FLAGS $INCLUDES $EXTRA_LINKER_ARGS *.o -o $EXPERIMENT_SUFFIX.bin"
RC=$?
echo linker return code: $RC
if [ $RC -ne 0 ]; then
  echo "Error: Failed to link"
  exit 5
fi
