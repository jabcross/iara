#ifndef IARA_RUNTIME_H
#define IARA_RUNTIME_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <shared_mutex>
#include <string>
#include <sys/cdefs.h>
#include <thread>
#include <typeinfo>
#include <unistd.h>
#include <unordered_map>
#include <vector>

extern const uint64_t iara_runtime_num_nodes;
extern const uint64_t iara_runtime_num_edges;

// C declarations

extern "C" void iara_runtime_processDependency(int64_t first_prod_iteration,
                                               int64_t last_prod_iteration,
                                               int64_t edge_id,
                                               uint8_t *buffer_root);

extern "C" void iara_runtime_createRuntime();

extern "C" void iara_runtime_alloc(int64_t node_id);

extern "C" void iara_runtime_dealloc(void *buffer);

namespace iara::runtime {

struct NodeData {
  int64_t input_port_count;
  int64_t input_bytes;
  int64_t output_port_count;
  int64_t total_firings;
  const int64_t *output_edges;
  // In the compiler, we keep the kernel function name. In the runtime,
  // it's a function pointer.
#ifndef IARA_RUNTIME
  std::string func_name;
#else
  void (*func_call)(int64_t cons_it, uint8_t **input_buffers);
#endif
};

struct EdgeData {
  int64_t prod_rate; // in bytes, -1 for alloc node
  int64_t prod_alpha;
  int64_t prod_beta;
  int64_t prod_id;
  int64_t cons_rate; // in bytes, -1 for dealloc node
  int64_t cons_alpha;
  int64_t cons_beta;
  int64_t cons_id;
  int64_t this_delay;
  int64_t remaining_delay;
  int64_t cons_input_port_index;
  int64_t buffer_size_with_delays;
  int64_t buffer_size_without_delays;

#ifdef IARA_RUNTIME
  uint8_t *init_buffer;
#endif

  inline bool isAlloc() const { return prod_rate == -1; }
  inline bool isDealloc() const { return cons_rate == -1; }
};

struct IterationDependencyCounter {
  int bytes_remaining; // actually atomic
  uint8_t **arguments = nullptr;
};

struct DependencyDict {
  // mutex used to prevent unordered map from mutating while atomic int is
  // being accessed;
  mutable std::shared_mutex mutex;
  std::unordered_map<int64_t, IterationDependencyCounter> counters;
};

// Vector with runtime data, indexed by actor id
template <size_t N> struct IaraRuntimeContext {
  std::array<DependencyDict, N> dependencies;
};

struct StaticData {
  int64_t num_nodes;
  int64_t num_edges;
  NodeData *nodes;
  EdgeData *edges;
};

} // namespace iara::runtime

#endif