#ifndef SIFT_CONFIG_H
#define SIFT_CONFIG_H

// I/O Configuration
#define SIFT_FILE_PATH_LENGTH 512
#define SIFT_DUMP_DESCRIPTOR 1
#define SIFT_DESCRIPTOR_BINS 128

// Image Configuration - Auto-generated from codegen.sh
#define SIFT_IMAGE_WIDTH 800
#define SIFT_IMAGE_HEIGHT 640
#define SIFT_IMG_DOUBLE 1
#define SIFT_TOT_IMAGE_SIZE 512000

// SIFT Algorithm Parameters - Auto-generated from codegen.sh
#define SIFT_GWIDTH_MAX 21
#define SIFT_PARALLELISM_LEVEL 4
#define SIFT_NUM_OCTAVES 7
#define SIFT_NUM_SCALES 5
#define SIFT_NUM_DOG_LAYERS 5
#define SIFT_NUM_GPYR_LAYERS 6
#define SIFT_OCTAVES_DOWN_N 4

#endif // SIFT_CONFIG_H
