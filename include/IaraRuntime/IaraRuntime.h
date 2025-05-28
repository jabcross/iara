#ifndef IARA_RUNTIME_H
#define IARA_RUNTIME_H

#include "Iara/Util/Types.h"
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

using std::byte;

extern const i64 iara_runtime_num_nodes;
extern const i64 iara_runtime_num_edges;

// C declarations

extern "C" void iara_runtime_processDependency(i64 first_prod_iteration,
                                               i64 last_prod_iteration,
                                               i64 edge_id, byte *buffer_root);

extern "C" void iara_runtime_createRuntime();

extern "C" void iara_runtime_alloc(i64 node_id);

extern "C" void iara_runtime_dealloc(void *buffer);

namespace iara::runtime {

struct NodeData {
  i64 input_port_count;
  i64 input_bytes;
  i64 output_port_count;
  i64 total_firings;
  const i64 *output_edges;
  // In the compiler, we keep the kernel function name. In the runtime,
  // it's a function pointer.
#ifndef IARA_RUNTIME
  std::string func_name;
#else
  void (*func_call)(i64 cons_it, byte **input_buffers);
  byte *init_buffer;
#endif
};

struct EdgeData {
  i64 prod_rate; // in bytes, -1 for alloc node
  i64 prod_alpha;
  i64 prod_beta;
  i64 prod_id;
  i64 cons_rate; // in bytes, -1 for dealloc node
  i64 cons_alpha;
  i64 cons_beta;
  i64 cons_id;
  i64 this_delay;
  i64 remaining_delay;
  i64 cons_input_port_index;
  i64 buffer_size_with_delays;
  i64 buffer_size_without_delays;
  i64 total_delay_size;
  char *delay_buffer;
  char *copyback_buffer;

  inline bool isAlloc() const { return prod_rate == -1; }
  inline bool isDealloc() const { return cons_rate == -1; }
};

struct IterationDependencyCounter {
  int bytes_remaining; // actually atomic
  byte **arguments = nullptr;
};

struct DependencyDict {
  // mutex used to prevent unordered map from mutating while atomic int is
  // being accessed;
  mutable std::shared_mutex mutex;
  std::unordered_map<i64, IterationDependencyCounter> counters;
};

// Vector with runtime data, indexed by actor id
template <size_t N> struct IaraRuntimeContext {
  std::array<DependencyDict, N> dependencies;
};

struct StaticData {
  i64 num_nodes;
  i64 num_edges;
  i64 num_buffers;
  NodeData *nodes;
  EdgeData *edges;
};

template <typename T, size_t N> struct MemRefDescriptor {
  T *allocated;
  T *aligned;
  intptr_t offset;
  intptr_t sizes[N];
  intptr_t strides[N];
};

} // namespace iara::runtime

#endif
