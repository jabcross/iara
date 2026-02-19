#!/bin/bash
# Generate SIFT configuration header file
# This script is executed by CMake during build
# It creates a header with SIFT-specific constants based on parameters

if [[ -z $PATH_TO_TEST_BUILD_DIR ]]; then
  echo "Error: Must specify PATH_TO_TEST_BUILD_DIR"
  exit 1
fi

# Parameters are provided by CMake from the experiment configuration via DEFINES
IMAGE_WIDTH=${IMAGE_WIDTH:-800}
IMAGE_HEIGHT=${IMAGE_HEIGHT:-640}
IMG_DOUBLE=${IMG_DOUBLE:-1}
G_WMAX=${G_WMAX:-21}
PARALLELISM_LEVEL=${PARALLELISM_LEVEL:-4}
N_GPYR_LAYERS=${N_GPYR_LAYERS:-6}

# Calculate total image size if not provided
if [[ -z $TOT_IMAGE_SIZE ]]; then
  TOT_IMAGE_SIZE=$((IMAGE_WIDTH * IMAGE_HEIGHT))
fi

# Fixed SIFT algorithm parameters
NUM_OCTAVES=7
NUM_SCALES=5
NUM_DOG_LAYERS=5
OCTAVES_DOWN_N=4

# Generate sift_config.h header file
cat > "$PATH_TO_TEST_BUILD_DIR/sift_config.h" << 'EOF'
#ifndef SIFT_CONFIG_H
#define SIFT_CONFIG_H

// I/O Configuration
#define SIFT_FILE_PATH_LENGTH 512
#define SIFT_DUMP_DESCRIPTOR 1
#define SIFT_DESCRIPTOR_BINS 128

// Image Configuration - Auto-generated from codegen.sh
EOF

cat >> "$PATH_TO_TEST_BUILD_DIR/sift_config.h" << EOF
#define SIFT_IMAGE_WIDTH $IMAGE_WIDTH
#define SIFT_IMAGE_HEIGHT $IMAGE_HEIGHT
#define SIFT_IMG_DOUBLE $IMG_DOUBLE
#define SIFT_TOT_IMAGE_SIZE $TOT_IMAGE_SIZE

// SIFT Algorithm Parameters - Auto-generated from codegen.sh
#define SIFT_GWIDTH_MAX $G_WMAX
#define SIFT_PARALLELISM_LEVEL $PARALLELISM_LEVEL
#define SIFT_NUM_OCTAVES $NUM_OCTAVES
#define SIFT_NUM_SCALES $NUM_SCALES
#define SIFT_NUM_DOG_LAYERS $NUM_DOG_LAYERS
#define SIFT_NUM_GPYR_LAYERS $N_GPYR_LAYERS
#define SIFT_OCTAVES_DOWN_N $OCTAVES_DOWN_N

#endif // SIFT_CONFIG_H
EOF

echo "Generated sift_config.h with parameters:"
echo "  IMAGE_WIDTH=$IMAGE_WIDTH"
echo "  IMAGE_HEIGHT=$IMAGE_HEIGHT"
echo "  IMG_DOUBLE=$IMG_DOUBLE"
echo "  G_WMAX=$G_WMAX"
echo "  PARALLELISM_LEVEL=$PARALLELISM_LEVEL"
echo "  N_GPYR_LAYERS=$N_GPYR_LAYERS"
echo "  TOT_IMAGE_SIZE=$TOT_IMAGE_SIZE"
