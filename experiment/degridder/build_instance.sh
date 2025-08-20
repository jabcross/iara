#!/bin/bash

if [[ $(basename $(realpath .)) -ne "build" ]]; then
  echo "Run this in instance build directory."
  exit 1
fi

if [[ -z $SCHEDULER_MODE ]]; then
  echo "Must provide SCHEDULER_MODE."
  exit 1
fi

build-iara.sh

INSTANCE_NAME=$(basename $(realpath ..))
export INSTANCE_BUILD_DIR=$(realpath .)
SOURCE_DIR=$IARA_DIR/experiment/degridder/Code
EXPERIMENT_DIR=$IARA_DIR/experiment/degridder

echo "DEBUG: Instance name parsing"
echo "  INSTANCE_NAME=$INSTANCE_NAME"
echo "  Current directory: $(pwd)"
echo "  Parent directory: $(realpath ..)"

# Function to instantiate templates
instantiate_template() {
  local original="$1"
  local instance="$2"
  local size="$3"
  local cores="$4"
  local num_kernel_support="$5"
  local num_chunks="$6"
  
  # Calculate parameters based on notebook logic
  local GRID_SIZE
  if [[ "$size" == "large" ]]; then
    GRID_SIZE=5120
  else
    GRID_SIZE=2560
  fi
  
  local NUM_KERNEL_SUPPORT=$num_kernel_support
  local OVERSAMPLING_FACTOR=16
  local NUMBER_SAMPLE_IN_KERNEL=$(( (NUM_KERNEL_SUPPORT + 1) * OVERSAMPLING_FACTOR * (NUM_KERNEL_SUPPORT + 1) * OVERSAMPLING_FACTOR ))
  local NUM_KERNELS=17
  
  local NUM_SCENARIO
  case "$size" in
    "small") NUM_SCENARIO=1 ;;
    "medium") NUM_SCENARIO=2 ;;
    "large") NUM_SCENARIO=3 ;;
    *) NUM_SCENARIO=1 ;;
  esac
  
  local NUM_VISIBILITIES
  if [[ "$size" == "large" ]]; then
    NUM_VISIBILITIES=7848960
  else
    NUM_VISIBILITIES=3924480
  fi
  
  local TOTAL_KERNELS_SAMPLES=$((NUM_KERNELS * NUMBER_SAMPLE_IN_KERNEL))
  local NUM_CHUNK=$num_chunks
  local NUM_VISIB_D_N_CHUNK=$((NUM_VISIBILITIES / NUM_CHUNK))
  
  # Verify division is exact
  if [[ $((NUM_VISIB_D_N_CHUNK * NUM_CHUNK)) -ne $NUM_VISIBILITIES ]]; then
    echo "Error: NUM_VISIBILITIES ($NUM_VISIBILITIES) is not divisible by NUM_CHUNK ($NUM_CHUNK)"
    exit 1
  fi
  
  # Remove existing instance file and create new one
  rm -f "$instance"
  
  # Debug: Print all calculated values
  echo "DEBUG: Template instantiation for $instance"
  echo "  GRID_SIZE=$GRID_SIZE"
  echo "  NUM_KERNEL_SUPPORT=$NUM_KERNEL_SUPPORT"
  echo "  OVERSAMPLING_FACTOR=$OVERSAMPLING_FACTOR"
  echo "  NUMBER_SAMPLE_IN_KERNEL=$NUMBER_SAMPLE_IN_KERNEL"
  echo "  NUM_KERNELS=$NUM_KERNELS"
  echo "  NUM_SCENARIO=$NUM_SCENARIO"
  echo "  NUM_VISIBILITIES=$NUM_VISIBILITIES"
  echo "  TOTAL_KERNELS_SAMPLES=$TOTAL_KERNELS_SAMPLES"
  echo "  NUM_CHUNK=$NUM_CHUNK"
  echo "  NUM_VISIB_D_N_CHUNK=$NUM_VISIB_D_N_CHUNK"

  # Apply substitutions using sed
  cat "$original" \
    | sed "s/GRID_SIZE_VALUE/$GRID_SIZE/g" \
    | sed "s/NUM_KERNEL_SUPPORT_VALUE/$NUM_KERNEL_SUPPORT/g" \
    | sed "s/OVERSAMPLING_FACTOR_VALUE/$OVERSAMPLING_FACTOR/g" \
    | sed "s/NUMBER_SAMPLE_IN_KERNEL_VALUE/$NUMBER_SAMPLE_IN_KERNEL/g" \
    | sed "s/NUM_KERNELS_VALUE/$NUM_KERNELS/g" \
    | sed "s/NUM_SCENARIO_VALUE/$NUM_SCENARIO/g" \
    | sed "s/NUM_VISIBILITIES_VALUE/$NUM_VISIBILITIES/g" \
    | sed "s/TOTAL_KERNELS_SAMPLES_VALUE/$TOTAL_KERNELS_SAMPLES/g" \
    | sed "s/NUM_CHUNK_VALUE/$NUM_CHUNK/g" \
    | sed "s/NUM_VISIB_D_N_CHUNK_VALUE/$NUM_VISIB_D_N_CHUNK/g" \
    > "$instance"
}

# Parse scenario parameters from instance name
# Format: P{N}_{size}_{cores}_{num_kernel_support}_{num_chunks}[_complete].scenario
if [[ $INSTANCE_NAME =~ P[0-9]+_([^_]+)_([^_]+)_([^_]+)_([^_]+).*\.scenario ]]; then
  SIZE="${BASH_REMATCH[1]}"
  CORES="${BASH_REMATCH[2]}"
  NUM_KERNEL_SUPPORT="${BASH_REMATCH[3]}"
  NUM_CHUNKS="${BASH_REMATCH[4]}"
  
  echo "Instantiating templates for: size=$SIZE, cores=$CORES, kernel_support=$NUM_KERNEL_SUPPORT, chunks=$NUM_CHUNKS"

  instantiate_template "$EXPERIMENT_DIR/topology.mlir.template" "topology.mlir" "$SIZE" "$CORES" "$NUM_KERNEL_SUPPORT" "$NUM_CHUNKS"

  instantiate_template "$EXPERIMENT_DIR/constants.h.template" "constants.h" "$SIZE" "$CORES" "$NUM_KERNEL_SUPPORT" "$NUM_CHUNKS"


else
  echo "Warning: Could not parse scenario parameters from instance name: $INSTANCE_NAME"
  echo "Expected format: P{N}_{size}_{cores}_{num_kernel_support}_{num_chunks}[_complete].scenario"
fi

MAIN_ACTOR_NAME=$(python -c "print('top_parallel_degridder_complete' if 'complete' in '$INSTANCE_NAME' else 'top_parallel_degridder')")

echo $MAIN_ACTOR_NAME

rm -f schedule.mlir
rm -f $EXPERIMENT_DIR/instances/$INSTANCE_NAME/iara_stderr.txt

/usr/bin/time -v -o "$EXPERIMENT_DIR/instances/$INSTANCE_NAME/iara_scheduling_time.txt" timeout 3m iara-opt --iara-canonicalize --flatten --$SCHEDULER_MODE=main-actor=$MAIN_ACTOR_NAME "topology.mlir" >"schedule.mlir" 2>$EXPERIMENT_DIR/instances/$INSTANCE_NAME/iara_stderr.txt


# Check if iara-opt succeeded
if [ $? -ne 0 ]; then
    echo "Error: iara-opt failed. Exiting."
    exit 1
fi

# Check if schedule.mlir exists and isn't empty
if [ ! -s "schedule.mlir" ]; then
  echo "Error: schedule.mlir is empty or doesn't exist. Exiting."
  exit 1
fi

# export EXPERIMENT_DIR=

sh -x mlir-to-llvmir.sh schedule.mlir

# Check if iara-opt succeeded
if [ $? -ne 0 ]; then
    echo "Error: mlir-to-llvmir.sh failed. Exiting."
    exit 1
fi

$LLVM_INSTALL/bin/llc -filetype=obj schedule.ll -o schedule.o

# Check if we're running from a build directory
if [[ $(basename $(pwd)) != "build" ]]; then
  echo "Error: This script must be run from a build directory."
  exit 1
fi
rm main.cpp
cp $EXPERIMENT_DIR/main.cpp main.cpp

cd ..

rm CMakeLists.txt
cp $EXPERIMENT_DIR/Code/CMakeLists.txt CMakeLists.txt

cmake --log-level=VERBOSE -B build 

cd build

make

# $LLVM_INSTALL/bin/clang -g -fopenmp -xir -c schedule.ll -o schedule.o

# $LLVM_INSTALL/bin/clang++ -std=c++26 -g -fopenmp -c -I$SOURCE_DIR/include main.cpp -o broadcasts.o

# OBJS=$(for i in fft_run degridding common top vis_to_csv; do echo -n "/home/jabcross/repos/degridder/Code/build/CMakeFiles/degridder_pipeline.dir/src/$i.c.o " ; done )

# $LLVM_INSTALL/bin/clang++ -stdlib=libc++ -L$LLVM_INSTALL/lib -lomp -lpthread -lc++ -lc++abi -L/usr/lib64 -L/usr/local/lib -lfftw3 *.o $OBJS
