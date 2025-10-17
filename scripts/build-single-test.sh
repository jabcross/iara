#!/bin/bash

set -x

FOLDER_NAME=$(basename $(realpath $1))

echo $PATH

# Check if exactly two arguments were provided
if [ "$#" -lt 1 ]; then
  echo "Pick test:"
  FOLDER_NAME=$(ls $IARA_DIR/test/Iara | fzf --prompt="Choose test")
fi

# Store arguments in variables

export PATH_TO_TEST_SOURCES=$IARA_DIR/test/Iara/$FOLDER_NAME

# Only define PATH_TO_TEST_BUILD_DIR if it's not already defined
if [ -z "$PATH_TO_TEST_BUILD_DIR" ]; then
  export PATH_TO_TEST_BUILD_DIR=$IARA_DIR/build/test/Iara/$FOLDER_NAME
fi

$IARA_DIR/scripts/build-iara.sh

mkdir -p $PATH_TO_TEST_BUILD_DIR

cd $PATH_TO_TEST_BUILD_DIR

# if [[ $(realpath ..) != "$IARA_DIR/build/test/Iara" ]]; then
#   echo -n "Wrong directory: "
#   pwd
#   exit 1
# fi

mkdir -p build
cd build

rm -rf *

rm *.o

echo "Building test: $FOLDER_NAME"
echo ""

if [[ $2 != "" ]]; then
  echo "Setting scheduler mode from argument"
  SCHEDULER_MODE=$2;
fi

echo Scheduler mode: \"$SCHEDULER_MODE\"

if [[ $SCHEDULER_MODE != virtual-fifo && $SCHEDULER_MODE != ring-buffer ]]; then
  echo "Not a IaRa scheduler mode. Setting it to virtual-fifo just to compile."
  # NOT_IARA=1
  SCHEDULER_MODE="virtual-fifo"
fi

if [ -z $LLVM_INSTALL ]; then
  echo "Must provide LLVM_INSTALL"
  exit 1
fi

if [ -f $PATH_TO_TEST_SOURCES/codegen.sh ]; then
  sh -x $PATH_TO_TEST_SOURCES/codegen.sh
  RC=$?
  if [[ $RC != 0 ]] ; then
    echo "Failed codegen script."
    exit 1
  fi
fi

CLANG_INCLUDE="$LLVM_INSTALL/lib/clang/21/include"

INCLUDES="-I$CLANG_INCLUDE -I. -I$IARA_DIR/include -I$IARA_DIR/external -I$PATH_TO_TEST_BUILD_DIR/build"

# MEMORY_SANITIZER_OPTIONS="-fsanitize=memory -fsanitize-memory-track-origins -fPIE -pie"

CPP_COMPILER=$LLVM_INSTALL/bin/clang++
C_COMPILER=$LLVM_INSTALL/bin/clang
# COMPILER_FLAGS="-stdlib=libc++ -fopenmp"
COMPILER_FLAGS="-stdlib=libc++ -fopenmp $MEMORY_SANITIZER_OPTIONS"
# COMPILER_FLAGS="-stdlib=libc++"
LINKER_FLAGS="-L$LLVM_INSTALL/lib -lomp -lpthread -lc++ -lc++abi $MEMORY_SANITIZER_OPTIONS"
# LINKER_FLAGS="-L$LLVM_INSTALL/lib -lc++ -lc++abi "

RUNTIME_FLAGS="-fopenmp"


# EXTRA_RUNTIME_ARGS=-DIARA_DEBUGPRINT

if [ -f "$PATH_TO_TEST_SOURCES/extra_args.sh" ]; then
  source "$PATH_TO_TEST_SOURCES/extra_args.sh"
fi

pwd
pwd >&2

if [[ $NOT_IARA == 1 ]]; then
  echo "External scheduler. Do not compile IaRa."
  rm schedule.o
  rm Common.o
  rm VirtualFIFO*.o
else

  echo "TOPOLOGY_FILE from env = «$TOPOLOGY_FILE»"

  if [[ $TOPOLOGY_FILE != "" ]]; then
    echo "Taking topology file from environment variable."
  elif [[ -f $PATH_TO_TEST_SOURCES/topology.mlir ]]; then
      TOPOLOGY_FILE="$PATH_TO_TEST_SOURCES/topology.mlir"
  elif [[ -f $PATH_TO_TEST_SOURCES/topology.test ]]; then
      TOPOLOGY_FILE="$PATH_TO_TEST_SOURCES/topology.test"
  elif [[ -f $PATH_TO_TEST_BUILD_DIR/build/topology.mlir ]]; then
      TOPOLOGY_FILE="$PATH_TO_TEST_BUILD_DIR/build/topology.mlir"
  else
      echo "Missing topology file."
      exit 1
  fi

  echo "Chosen topology file = «$TOPOLOGY_FILE»"

  echo SCHEDULER_MODE = \"$SCHEDULER_MODE\"

  SCHEDULER_SOURCES="-c $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* "

  echo SCHEDULER_SOURCES = \"$SCHEDULER_SOURCES\"

  # if [ "$SCHEDULER_MODE" == "virtual-fifo" ]; then
  #   SCHEDULER_SOURCES="-c $IARA_DIR/runtime/SDF_OoO*.cpp"
  # else
  #   echo "No recognized scheduler mode: --$SCHEDULER_MODE".
  #   exit 1
  # fi

  if [[ $IARA_FLAGS == "" ]]; then
    IARA_FLAGS="--iara-canonicalize --flatten --$SCHEDULER_MODE='main-actor=run'"
  fi

  echo Topology file = $TOPOLOGY_FILE

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
  \time -f 'compiling schedule took %E and returned code %x' bash -xc "$CPP_COMPILER -ftime-trace=schedule.json --std=c++20 -g $COMPILER_FLAGS $EXTRA_SCHEDULE_ARGS $INCLUDES -xir -c schedule.ll -o schedule.o"
  RC=$?
  echo scheduler return code: $?
  if [ $RC -ne 0 ]; then
    echo "Error: Failed to build schedule"
    exit 1
  fi

  ls ..

  pwd >&2

    echo building runtime
  \time -f 'compiling runtime took %E and returned code %x' bash -xc "$CPP_COMPILER -ftime-trace=runtime.json --std=c++20 -g $COMPILER_FLAGS $RUNTIME_FLAGS $EXTRA_RUNTIME_ARGS $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
  # \time -f 'compiling runtime took %E and returned code %x' bash -xc "$CPP_COMPILER --std=c++20 -g -xc++ -std=c++20  $SCHEDULER_SOURCES $INCLUDES"
  RC=$?
  echo executable return code: $?
  if [ $RC -ne 0 ]; then
    echo "Error: Failed to build runtime"
    exit 4
  fi
fi

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
    \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "$CPP_COMPILER -g $INCLUDES -xc++ -std=c++20 -ftime-trace=cppkernels.json $COMPILER_FLAGS $EXTRA_KERNEL_ARGS $INCLUDES -c $cpp_file $SCHEDULER_SOURCES "
    RC=$?
    echo cpp kernels return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build cpp files"
      exit 3
    fi
  done
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
