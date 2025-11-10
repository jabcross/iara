#!/bin/bash

FOLDER_NAME=$(basename $(realpath $1))

echo $FOLDER_NAME

cd $IARA_DIR

# Runs a test (directly, not through lit)

set -x

# Check if exactly two arguments were provided
if [ "$#" -lt 1 ]; then
  echo "Pick test:"
  FOLDER_NAME=$(ls $IARA_DIR/test/Iara | fzf --prompt="Choose test")
fi

# Get scheduler mode from second argument or use default
SCHEDULER_MODE=${2:-virtual-fifo}

# Store arguments in variables
PATH_TO_TEST_SOURCES=$IARA_DIR/test/Iara/$FOLDER_NAME
PATH_TO_TEST_BUILD_DIR=$IARA_DIR/build/test/Iara/$FOLDER_NAME

# Use scheduler-specific build directory
BUILD_SUBDIR="build-$SCHEDULER_MODE"

# Navigate to the build directory
cd $PATH_TO_TEST_BUILD_DIR/$BUILD_SUBDIR

if [[ ! -f ./a.out ]]; then
  echo "Error: Executable not found. Build may have failed."
  exit 1
fi

echo "==== Running test executable ===="

# Run the test with timing - color function needs to be available in the subshell
\time -f 'Test execution took %E and returned code %x' bash -c './a.out'
