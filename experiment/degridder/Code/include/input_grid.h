#ifndef SPLITMERGE_H
#define SPLITMERGE_H

#include "common.h"

#ifndef CPU_VERSION
  #define CPU_VERSION 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

void input_grid_setup(Config *config, float2 *output);

#ifdef __cplusplus
}
#endif

#endif
