#include <stdio.h>
#include <string.h>

#include "ezsift-preesm.h"

#include "sift_config.h"

void filename1(OUT char *filename) {
#ifdef SIFT_DEBUG
  fprintf(stderr, "Enter function: %s\n", __FUNCTION__);
#endif
  //  strncpy(filename,
  //  "/home/ahonorat/Documents/imagesTestsSIFT/bike-3840x2400.pgm", (size_t)
  //  SIFT_FILE_PATH_LENGTH);
  strncpy(filename, "../applications/08-sift/data/img1.pgm", (size_t)SIFT_FILE_PATH_LENGTH);
  // strncpy(filename, "../applications/08-sift/data/down2x1.pgm", (size_t) SIFT_FILE_PATH_LENGTH);
}
