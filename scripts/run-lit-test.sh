#!/bin/bash

# pass a path to a test; this will run the corresponding lit test

cd $1

TEST_NAME=${PWD##*/}

if [[ $2 == '--ignore-result' ]]; then
	llvm-lit "${@:3}" "${IARA_DIR}/build/test/Iara/${TEST_NAME}"
	exit 0
else
	llvm-lit "${@:2}" "${IARA_DIR}/build/test/Iara/${TEST_NAME}"
fi
