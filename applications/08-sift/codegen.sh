#!/bin/bash
# Generate SIFT configuration header file.
# Called by CMake during the setup phase of each experiment instance.
# Image size is fixed at 800x640.

if [[ -z $PATH_TO_TEST_BUILD_DIR ]]; then
  echo "Error: Must specify PATH_TO_TEST_BUILD_DIR"
  exit 1
fi

# Fixed image dimensions (topology.mlir is hardcoded for 800x640)
IMAGE_WIDTH=800
IMAGE_HEIGHT=640

# Fixed SIFT algorithm parameters
IMG_DOUBLE=${IMG_DOUBLE:-1}
G_WMAX=${G_WMAX:-21}
PARALLELISM_LEVEL=${NUM_CORES:-${PARALLELISM_LEVEL:-4}}
N_GPYR_LAYERS=${N_GPYR_LAYERS:-6}
TOT_IMAGE_SIZE=$((IMAGE_WIDTH * IMAGE_HEIGHT))
IMAGE_MAXWH=$((IMAGE_WIDTH > IMAGE_HEIGHT ? IMAGE_WIDTH : IMAGE_HEIGHT))
NUM_OCTAVES=7
NUM_SCALES=5
NUM_DOG_LAYERS=5
NLAYERS=$((NUM_DOG_LAYERS - 2))  # nLayers from Htop_sift.pi: <node expr="3" id="nLayers"/>
OCTAVES_DOWN_N=4

# Generate sift_config.h
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
#define SIFT_IMAGE_MAXWH $IMAGE_MAXWH

// SIFT Algorithm Parameters - Auto-generated from codegen.sh
#define SIFT_GWIDTH_MAX $G_WMAX
#define SIFT_PARALLELISM_LEVEL $PARALLELISM_LEVEL
#define SIFT_NUM_OCTAVES $NUM_OCTAVES
#define SIFT_NUM_SCALES $NUM_SCALES
#define SIFT_NUM_DOG_LAYERS $NUM_DOG_LAYERS
#define SIFT_NUM_GPYR_LAYERS $N_GPYR_LAYERS
#define SIFT_OCTAVES_DOWN_N $OCTAVES_DOWN_N
#define SIFT_NLAYERS $NLAYERS

// File path for the input image (relative to build dir working directory)
#define SIFT_IMG_PATH "../applications/08-sift/data/img1.pgm"

#endif // SIFT_CONFIG_H
EOF

echo "Generated sift_config.h: ${IMAGE_WIDTH}x${IMAGE_HEIGHT}, parallelism=$PARALLELISM_LEVEL"

# Generate topology.mlir for this parallelism level
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
IARA_DIR="$(realpath "${SCRIPT_DIR}/../..")"
TOPOLOGY_MLIR="${PATH_TO_TEST_BUILD_DIR}/topology.mlir"

python3 "${IARA_DIR}/scripts/instantiate-mlir-template.py" \
    "${SCRIPT_DIR}/preesm-params.json" \
    "${SCRIPT_DIR}/blur2x.mlir.template" \
    "${SCRIPT_DIR}/blur.mlir.template" \
    "${SCRIPT_DIR}/Htop_sift.mlir.template" \
    "${SCRIPT_DIR}/Hextract.mlir.template" \
    "${SCRIPT_DIR}/preesm2iara.c.template" \
    "NUM_CORES=${PARALLELISM_LEVEL}" \
    -o "${TOPOLOGY_MLIR}" \
    --wrappers-out "${PATH_TO_TEST_BUILD_DIR}/preesm2iara.c"

# Create applications/ symlink in parent of build dir so that the relative
# data path in src/filenames.c ("../applications/08-sift/data/img1.pgm")
# resolves correctly when a.out is executed from PATH_TO_TEST_BUILD_DIR.
if [[ -n "${IARA_DIR:-}" ]]; then
  PARENT_DIR="$(dirname "$PATH_TO_TEST_BUILD_DIR")"
  if [ ! -e "${PARENT_DIR}/applications" ]; then
    ln -sfn "${IARA_DIR}/applications" "${PARENT_DIR}/applications"
    echo "Created symlink: ${PARENT_DIR}/applications -> ${IARA_DIR}/applications"
  fi
fi
