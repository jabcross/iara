#!/bin/bash

cd $IARA_DIR

# Runs a test (directly, not through lit)

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

if [[ ! -d $PATH_TO_TEST_BUILD_DIR ]]; then
  $IARA_DIR/scripts/build-single-test.sh $PATH_TO_TEST_SOURCES
fi

cd $PATH_TO_TEST_BUILD_DIR

if [[ $(basename $(realpath ..)) != "Iara" ]]; then
  echo "Wrong directory!"
  exit 1
fi

mkdir -p build
cd build

build-single-test.sh $PATH_TO_TEST_SOURCES

color() (
  set -o pipefail
  "$@" 2>&1 >&3 | sed $'s,.*,    \e[31m> &\e[m,' >&2
) 3>&1

color ./a.out
