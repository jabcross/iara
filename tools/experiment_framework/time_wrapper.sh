#!/bin/bash
# Wrapper for GNU time with standardized output
#
# Usage: time_wrapper.sh <tool> <output_file> <command...>
#
# Example:
#   time_wrapper.sh iara-opt timing.txt iara-opt -o output.mlir input.mlir

TOOL=$1
OUTPUT_FILE=$2
shift 2  # Remove first two arguments
COMMAND="$@"

# Run command with GNU time
/usr/bin/time -v -o "${OUTPUT_FILE}" ${COMMAND}
EXIT_CODE=$?

# Check if time output was created
if [ ! -f "${OUTPUT_FILE}" ]; then
    echo "ERROR: GNU time output not created: ${OUTPUT_FILE}" >&2
    exit 1
fi

exit ${EXIT_CODE}
