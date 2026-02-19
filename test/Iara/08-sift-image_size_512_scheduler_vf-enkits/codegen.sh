#!/bin/bash
# Generate SIFT parameters header file
# This script is executed by CMake during build
# It creates a header with SIFT-specific constants based on IMAGE_SIZE

if [[ -z $PATH_TO_TEST_BUILD_DIR ]]; then
  echo "Error: Must specify PATH_TO_TEST_BUILD_DIR"
  exit 1
fi

# IMAGE_SIZE is provided by CMake from the parameter
IMAGE_SIZE=${IMAGE_SIZE:-512}

# Calculate SIFT parameters based on image size
# Standard SIFT configuration
NUM_OCTAVES=4
NUM_SCALES=5
GWIDTH_MAX=$((IMAGE_SIZE / 2))
NUM_GPYR_LAYERS=$((NUM_SCALES - 1))
NUM_LAYERS=$NUM_SCALES

# Generate sift_params.h header file
cat > "$PATH_TO_TEST_BUILD_DIR/sift_params.h" << 'EOF'
#ifndef SIFT_PARAMS_H
#define SIFT_PARAMS_H

// Auto-generated SIFT parameters

// Image dimensions
#define SIFT_IMAGE_WIDTH IMAGE_SIZE
#define SIFT_IMAGE_HEIGHT IMAGE_SIZE

// Pyramid configuration
#define SIFT_GWIDTH_MAX (IMAGE_SIZE / 2)
#define SIFT_NUM_GPYR_LAYERS (SIFT_NUM_SCALES - 1)
#define SIFT_NUM_SCALES 5
#define SIFT_NUM_LAYERS SIFT_NUM_SCALES
#define SIFT_NUM_OCTAVES 4
#define SIFT_IMG_DOUBLE 0

#endif // SIFT_PARAMS_H
EOF

echo "Generated sift_params.h with IMAGE_SIZE=$IMAGE_SIZE"
