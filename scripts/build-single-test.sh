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

# Use CMake to build iara-opt only if needed (CMake handles dependency tracking)
echo "==== Building iara-opt ===="
\time -f 'iara-opt build took %E' cmake --build $IARA_DIR/build --target iara-opt

mkdir -p $PATH_TO_TEST_BUILD_DIR

cd $PATH_TO_TEST_BUILD_DIR

# if [[ $(realpath ..) != "$IARA_DIR/build/test/Iara" ]]; then
#   echo -n "Wrong directory: "
#   pwd
#   exit 1
# fi

echo "Building test: $FOLDER_NAME"
echo ""

if [[ $2 != "" ]]; then
  echo "Setting scheduler mode from argument"
  SCHEDULER_MODE=$2;
fi

echo Scheduler mode: \"$SCHEDULER_MODE\"

# Check if this is an IaRa scheduler or external (OpenMP) scheduler
if [[ $SCHEDULER_MODE == "sequential" || $SCHEDULER_MODE == "omp-for" || $SCHEDULER_MODE == "omp-task" ]]; then
  echo "External/non-IaRa scheduler. Will not compile IaRa runtime."
  NOT_IARA=1
  # For non-IaRa builds, use a separate build directory to keep them isolated
  BUILD_SUBDIR="build-$SCHEDULER_MODE"
elif [[ $SCHEDULER_MODE == "virtual-fifo" || $SCHEDULER_MODE == "ring-buffer" ]]; then
  echo "IaRa scheduler mode."
  NOT_IARA=0
  BUILD_SUBDIR="build-$SCHEDULER_MODE"
else
  echo "Unknown scheduler mode: $SCHEDULER_MODE"
  exit 1
fi

mkdir -p "$BUILD_SUBDIR"
cd "$BUILD_SUBDIR"

# Only remove the executable - keep object files and generated MLIR files
# Dependency tracking below will rebuild only what's needed
rm -f a.out

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

INCLUDES="-I$CLANG_INCLUDE -I. -I$IARA_DIR/include -I$IARA_DIR/external -I$PATH_TO_TEST_BUILD_DIR/$BUILD_SUBDIR"

# MEMORY_SANITIZER_OPTIONS="-fsanitize=memory -fsanitize-memory-track-origins -fPIE -pie"

# Use BUILD_OPTIMIZATION if set, otherwise default to -g for debug
if [ -z "$BUILD_OPTIMIZATION" ]; then
  BUILD_OPTIMIZATION="-g"
fi

# Check if ccache should be used
# By default, use ccache if available unless USE_CCACHE is explicitly set to 0
if [ -z "$USE_CCACHE" ]; then
  # Not set - use ccache if available (default behavior)
  if command -v ccache &> /dev/null; then
    echo "Using ccache for faster compilation (default)"
    CCACHE="ccache"
  else
    CCACHE=""
  fi
elif [ "$USE_CCACHE" = "1" ]; then
  # Explicitly enabled
  if command -v ccache &> /dev/null; then
    echo "Using ccache for faster compilation (enabled via USE_CCACHE)"
    CCACHE="ccache"
  else
    echo "WARNING: ccache requested but not found, proceeding without it"
    CCACHE=""
  fi
else
  # Explicitly disabled (e.g., for compile time measurement)
  echo "ccache disabled (USE_CCACHE not set to 1)"
  CCACHE=""
fi

CPP_COMPILER=$LLVM_INSTALL/bin/clang++
C_COMPILER=$LLVM_INSTALL/bin/clang
# COMPILER_FLAGS="-stdlib=libc++ -fopenmp"
COMPILER_FLAGS="-stdlib=libc++ -fopenmp $BUILD_OPTIMIZATION $MEMORY_SANITIZER_OPTIONS"
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
  elif [[ -f $PATH_TO_TEST_BUILD_DIR/$BUILD_SUBDIR/topology.mlir ]]; then
      TOPOLOGY_FILE="$PATH_TO_TEST_BUILD_DIR/$BUILD_SUBDIR/topology.mlir"
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

  echo "==== Checking schedule.mlir ===="
  # Check if we need to rebuild schedule.mlir
  NEED_REBUILD_SCHEDULE=0
  if [ ! -f "schedule.mlir" ]; then
    NEED_REBUILD_SCHEDULE=1
    echo "schedule.mlir not found, will rebuild"
  elif [ "$TOPOLOGY_FILE" -nt "schedule.mlir" ]; then
    NEED_REBUILD_SCHEDULE=1
    echo "Topology file is newer than schedule.mlir, will rebuild"
  elif [ "$IARA_DIR/build/bin/iara-opt" -nt "schedule.mlir" ]; then
    NEED_REBUILD_SCHEDULE=1
    echo "iara-opt is newer than schedule.mlir, will rebuild"
  else
    echo "✓ schedule.mlir is up to date, SKIPPED"
  fi

  if [ $NEED_REBUILD_SCHEDULE -eq 1 ]; then
    \time -f 'iara-opt took %E and returned code %x' bash -xc "iara-opt $IARA_FLAGS $TOPOLOGY_FILE >schedule.mlir 2>/dev/null"
    RC=$?
    echo iara-opt return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to run iara-opt"
      exit 1
    fi
  fi

  echo "==== Checking schedule.ll ===="
  # Check if we need to rebuild schedule.ll
  NEED_REBUILD_LL=0
  if [ ! -f "schedule.ll" ]; then
    NEED_REBUILD_LL=1
    echo "schedule.ll not found, will rebuild"
  elif [ "schedule.mlir" -nt "schedule.ll" ]; then
    NEED_REBUILD_LL=1
    echo "schedule.mlir is newer than schedule.ll, will rebuild"
  else
    echo "✓ schedule.ll is up to date, SKIPPED"
  fi

  if [ $NEED_REBUILD_LL -eq 1 ]; then
    \time -f 'mlir-to-llvmir took %E and returned code %x' sh -x mlir-to-llvmir.sh schedule.mlir
    RC=$?
    echo mlir-to-llvmir return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to convert to llvm ir"
      exit 1
    fi
  fi

  shopt -s nullglob

  echo "==== Checking schedule.o ===="
  # Check if we need to rebuild schedule.o
  NEED_REBUILD_SCHEDULE_O=0
  if [ ! -f "schedule.o" ]; then
    NEED_REBUILD_SCHEDULE_O=1
    echo "schedule.o not found, will rebuild"
  elif [ "schedule.ll" -nt "schedule.o" ]; then
    NEED_REBUILD_SCHEDULE_O=1
    echo "schedule.ll is newer than schedule.o, will rebuild"
  else
    echo "✓ schedule.o is up to date, SKIPPED"
  fi

  if [ $NEED_REBUILD_SCHEDULE_O -eq 1 ]; then
    echo building schedule
    \time -f 'compiling schedule took %E and returned code %x' bash -xc "$CCACHE $CPP_COMPILER -ftime-trace=schedule.json --std=c++20 -g $COMPILER_FLAGS $EXTRA_SCHEDULE_ARGS $INCLUDES -xir -c schedule.ll -o schedule.o"
    RC=$?
    echo scheduler return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build schedule"
      exit 1
    fi
  fi

  ls ..

  pwd >&2

  echo "==== Checking runtime objects ===="
  # Check if we need to rebuild runtime objects
  NEED_REBUILD_RUNTIME=0
  RUNTIME_OBJS=$(basename -a $IARA_DIR/runtime/$SCHEDULER_MODE/*.c* | sed 's/\.\(cpp\|c\)$/.o/')
  
  for obj in $RUNTIME_OBJS; do
    if [ ! -f "$obj" ]; then
      NEED_REBUILD_RUNTIME=1
      echo "Runtime object $obj not found, will rebuild runtime"
      break
    fi
  done
  
  if [ $NEED_REBUILD_RUNTIME -eq 0 ]; then
    for src in $IARA_DIR/runtime/$SCHEDULER_MODE/*.c*; do
      obj=$(basename "$src" | sed 's/\.\(cpp\|c\)$/.o/')
      if [ "$src" -nt "$obj" ]; then
        NEED_REBUILD_RUNTIME=1
        echo "Runtime source $(basename $src) is newer than $obj, will rebuild runtime"
        break
      fi
    done
  fi

  if [ $NEED_REBUILD_RUNTIME -eq 1 ]; then
    echo building runtime
    \time -f 'compiling runtime took %E and returned code %x' bash -xc "$CCACHE $CPP_COMPILER -ftime-trace=runtime.json --std=c++20 -g $COMPILER_FLAGS $RUNTIME_FLAGS $EXTRA_RUNTIME_ARGS $INCLUDES -xc++ -std=c++20 $SCHEDULER_SOURCES"
    RC=$?
    echo executable return code: $?
    if [ $RC -ne 0 ]; then
      echo "Error: Failed to build runtime"
      exit 4
    fi
  else
    echo "✓ Runtime objects are up to date, SKIPPED"
  fi
fi

echo "==== Building C kernels ===="
if [ "$(ls $PATH_TO_TEST_SOURCES/*.c 2>/dev/null)" ]; then
  for c_file in $PATH_TO_TEST_SOURCES/*.c; do
    obj_file=$(basename "$c_file" .c).o
    
    # Check if object file needs rebuilding
    if [ ! -f "$obj_file" ] || [ "$c_file" -nt "$obj_file" ]; then
      echo "Compiling $c_file"
      \time -f 'compiling c kernels took %E and returned code %x' bash -xc "$CCACHE $CPP_COMPILER -g $COMPILER_FLAGS -ftime-trace=ckernels.json $INCLUDES $EXTRA_KERNEL_ARGS -xc -c "$c_file" "
      RC=$?
      echo c kernels return code: $?
      if [ $RC -ne 0 ]; then
        echo "Error: Failed to build c files"
        exit 2
      fi
    else
      echo "✓ Object file $obj_file is up to date, SKIPPED"
    fi
  done
fi

echo "==== Building C++ kernels ===="
if [ "$(ls $PATH_TO_TEST_SOURCES/*.cpp 2>/dev/null)" ]; then
  for cpp_file in $PATH_TO_TEST_SOURCES/*.cpp; do
    obj_file=$(basename "$cpp_file" .cpp).o
    
    # Check if object file needs rebuilding
    if [ ! -f "$obj_file" ] || [ "$cpp_file" -nt "$obj_file" ]; then
      echo "Compiling $cpp_file"
      \time -f 'compiling cpp kernels took %E and returned code %x' bash -xc "$CPP_COMPILER -g $INCLUDES -xc++ -std=c++20 -ftime-trace=cppkernels.json $COMPILER_FLAGS $EXTRA_KERNEL_ARGS $INCLUDES -c $cpp_file $SCHEDULER_SOURCES "
      RC=$?
      echo cpp kernels return code: $?
      if [ $RC -ne 0 ]; then
        echo "Error: Failed to build cpp files"
        exit 3
      fi
    else
      echo "✓ Object file $obj_file is up to date, SKIPPED"
    fi
  done
fi

echo "==== Linking ===="
EXTRAOBJS=""

for DIR in $EXTRA_OBJ_DIRS; do
  EXTRAOBJS="$EXTRAOBJS $PATH_TO_TEST_SOURCES/$DIR/*.o"
done

# Check if we need to relink
NEED_RELINK=0
if [ ! -f "a.out" ]; then
  NEED_RELINK=1
  echo "Executable not found, will link"
else
  # Check if any object file is newer than the executable
  for obj in *.o $EXTRAOBJS; do
    if [ -f "$obj" ] && [ "$obj" -nt "a.out" ]; then
      NEED_RELINK=1
      echo "Object file $obj is newer than a.out, will relink"
      break
    fi
  done
fi

if [ $NEED_RELINK -eq 1 ]; then
  echo linking
  \time -f 'linking took %E and returned code %x' bash -xc "$CPP_COMPILER --std=c++20 -g -fuse-ld=mold $LINKER_FLAGS $LDFLAGS $INCLUDES $EXTRA_LINKER_ARGS *.o $EXTRAOBJS"
  RC=$?
  echo linker return code: $?
  if [ $RC -ne 0 ]; then
    echo "Error: Failed to link"
    exit 5
  fi
else
  echo "✓ Executable a.out is up to date, SKIPPED"
fi
