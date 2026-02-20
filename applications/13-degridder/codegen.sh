# This file is executed by the CMake build system during instance builds
# It generates the topology.mlir file for degridder based on instance parameters

echo "Generating degridder topology.mlir"

# Extract parameters from environment variables (set by CMake)
if [[ -z $SIZE ]]; then
  echo "ERROR: Must specify SIZE (small, medium, or large)"
  exit 1
fi

if [[ -z $NUM_KERNEL_SUPPORT ]]; then
  echo "ERROR: Must specify NUM_KERNEL_SUPPORT"
  exit 1
fi

if [[ -z $NUM_CHUNK ]]; then
  echo "ERROR: Must specify NUM_CHUNK"
  exit 1
fi

echo "SIZE = $SIZE"
echo "NUM_KERNEL_SUPPORT = $NUM_KERNEL_SUPPORT"
echo "NUM_CHUNK = $NUM_CHUNK"

# Call the topology generation script and redirect output to topology.mlir
python3 "${IARA_DIR}/applications/13-degridder/generate_topology.py" "$SIZE" "$NUM_KERNEL_SUPPORT" "$NUM_CHUNK" > topology.mlir

if [ $? -ne 0 ]; then
  echo "ERROR: Failed to generate topology.mlir"
  exit 1
fi

echo "Successfully generated topology.mlir"

# Generate generated_data_path.h
DATA_ROOT_PATH="$IARA_DIR/applications/13-degridder/data"
GENERATED_HEADER="$PATH_TO_TEST_BUILD_DIR/generated_data_path.h"

echo "#ifndef GENERATED_DATA_PATH_H" > "$GENERATED_HEADER"
echo "#define GENERATED_DATA_PATH_H" >> "$GENERATED_HEADER"
echo "" >> "$GENERATED_HEADER"
echo "#define DATA_ROOT_PATH \"$DATA_ROOT_PATH\"" >> "$GENERATED_HEADER"
echo "" >> "$GENERATED_HEADER"
echo "#endif // GENERATED_DATA_PATH_H" >> "$GENERATED_HEADER"

echo "Successfully generated $GENERATED_HEADER"

# Create output directory for degridder results
mkdir -p output
