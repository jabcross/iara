#!/bin/bash

set -x

# pass a path to a test; this will run the corresponding lit test

cd $1

TEST_NAME=${PWD##*/}

CLEAN_FIRST=0
IGNORE_RESULT=0

cd $IARA_DIR

export SCHEDULER_MODE=ooo-scheduler

# Parse arguments
for arg in "$@"; do
	if [[ $arg == '--clean-first' ]]; then
		CLEAN_FIRST=1
	elif [[ $arg == '--ignore-result' ]]; then
		IGNORE_RESULT=1
	fi
done

# Remove the first argument (path) and the flags
REMAINING_ARGS=()
for arg in "${@:2}"; do
	if [[ $arg != '--clean-first' && $arg != '--ignore-result' ]]; then
		REMAINING_ARGS+=("$arg")
	fi
done

# Clean first if requested
if [[ $CLEAN_FIRST -eq 1 ]]; then
	rm -rf ${IARA_DIR}/build/test/Iara/${TEST_NAME}/*
	rm -rf ${IARA_DIR}/test/Iara/${TEST_NAME}/build/*
fi

# Run the test
if [[ $IGNORE_RESULT -eq 1 ]]; then
	llvm-lit -a "${REMAINING_ARGS[@]}" "${IARA_DIR}/build/test/Iara/${TEST_NAME}"
	exit 0
else
	llvm-lit -a "${REMAINING_ARGS[@]}" "${IARA_DIR}/build/test/Iara/${TEST_NAME}"
fi
