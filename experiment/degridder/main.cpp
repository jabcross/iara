#include "IaraRuntime/common/Scheduler.h"
#include "common.h"
#include "constants.h"
#include <algorithm>
#include <cassert>
#include <omp.h>
#include <span>

// Use the constants from constants.h
constexpr size_t total_kernels_samples = TOTAL_KERNELS_SAMPLES;
constexpr size_t num_chunk = NUM_CHUNK;
constexpr size_t grid_size = GRID_SIZE;
constexpr size_t num_kernels = NUM_KERNELS;
constexpr size_t num_visibilities = NUM_VISIBILITIES;

template <class T> void _broadcast(std::span<T> in, std::span<T> out) {
  assert(out.size() % in.size() == 0);
  for (auto i = std::begin(out), e = std::end(out); i < e; i += in.size()) {
    std::copy(std::begin(in), std::end(in), i);
  }
}

// Broadcast impls
extern "C" {

void broadcast_kernels_parallel(float2 in[total_kernels_samples],
                                float2 out[num_chunk * total_kernels_samples]) {
  _broadcast(std::span(in, total_kernels_samples),
             std::span(out, num_chunk * total_kernels_samples));
}

void broadcast_support_parallel(int2 in[num_kernels],
                                int2 out[num_chunk * num_kernels]) {
  _broadcast(std::span(in, num_kernels),
             std::span(out, num_chunk * num_kernels));
}

void broadcast_input_grid(float2 in[grid_size * grid_size],
                          float2 out[num_chunk * grid_size * grid_size]) {
  _broadcast(std::span(in, grid_size * grid_size),
             std::span(out, num_chunk * grid_size * grid_size));
}

void broadcast_uvw_coord(float3 in[num_visibilities],
                         float3 out[num_chunk * num_visibilities]) {
  _broadcast(std::span(in, num_visibilities),
             std::span(out, num_chunk * num_visibilities));
}

void broadcast_config_parallel(Config in[1], Config out[num_chunk]) {
  _broadcast(std::span(in, 1), std::span(out, num_chunk));
}

void exec() {
  iara_runtime_init();

  iara_runtime_run_iteration(0);
}
}

int main() {

  iara_runtime_exec(exec);

  return 0;
}