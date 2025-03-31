#include <cstdio>
#include <cstring>
#define IARA_RUNTIME

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <shared_mutex>

#include "IaraRuntime/IaraRuntime.h"
#include "edge_data.inc.h"

using namespace iara::runtime;

extern const StaticData iara_runtime_static_data;
std::atomic_int iara_deallocd_buffers;
std::atomic_bool iara_runtime_graph_iteration_finished = false;

using namespace iara::runtime;

DependencyDict *iara_runtime_dependency_dicts;

// calculates which iterations of the consumer depend on a a given iteration of
// the producer
struct Bounds {
  int64_t first, last;
  Bounds(int64_t first, int64_t last) : first(first), last(last) {}
  Bounds(int64_t single) : first(single), last(single) {}
  auto begin() { return first; }
  auto end() { return last + 1; }

  Bounds clip(Bounds other) {
    return {std::max(first, other.first), std::min(last, other.last)};
  }

  int64_t size() { return std::max(0l, last - first + 1); }
};

int64_t getTotalBytes(const EdgeData &edge, int64_t cons_it) {
  if (edge.isDealloc()) {
    if (cons_it == 0) {
      return edge.buffer_size_with_delays;
    }
    return edge.buffer_size_without_delays;
  }
  return iara_runtime_static_data.nodes[edge.cons_id].input_bytes;
}

// Calculates the buffer generation, given a producer firing number.
// Zero for the first allocation (with delays), and increments for subsequent
// ones.
int64_t get_buffer_generation(EdgeData const &edge, int64_t prod_iteration) {
  assert(edge.prod_rate != -1);
  if (prod_iteration < edge.prod_alpha)
    return 0;
  return (prod_iteration - edge.prod_alpha) / edge.prod_beta + 1;
}

// returns buffer epoch (0 if first execution(that runs with delays), >0 if
// after delays) and offset in bytes from start of buffer.
static std::pair<int64_t, Bounds>
get_buffer_offset_cons(EdgeData const &edge, int64_t cons_iteration) {
  assert(not edge.isDealloc());
  if (cons_iteration < edge.cons_alpha) {
    auto lb = edge.remaining_delay + cons_iteration * edge.cons_rate;
    return {0, {lb, lb + edge.cons_rate - 1}};
  }
  auto [buffer_epoch, offset] =
      lldiv(cons_iteration - edge.cons_alpha, edge.cons_beta);
  return {buffer_epoch + 1, {offset, offset + edge.cons_rate - 1}};
}

// returns buffer epoch (0 if first execution(that runs with delays), >0 if
// after delays) and offset in bytes from start of buffer.
static std::pair<int64_t, Bounds>
get_buffer_offset_prod(EdgeData const &edge, int64_t prod_iteration) {
  assert(not edge.isAlloc());
  if (prod_iteration < edge.prod_alpha) {
    auto lb = edge.remaining_delay + edge.this_delay +
              prod_iteration * edge.prod_rate;
    return {0, {lb, lb + edge.prod_rate - 1}};
  }
  auto [buffer_epoch, offset] =
      lldiv(prod_iteration - edge.prod_alpha, edge.prod_beta);
  return {buffer_epoch + 1, {offset, offset + edge.prod_rate - 1}};
}

// Maps a range of producer iteration numbers to a range of consumer firing
// numbers. Assumes not alloc or free.
static inline Bounds prod_to_cons(EdgeData const &edge, int64_t prod_it) {

  int64_t all_delays, iter_this_buffer, buffer_epoch;

  if (prod_it < edge.prod_alpha) {
    all_delays = edge.this_delay + edge.remaining_delay;
    iter_this_buffer = prod_it;
    buffer_epoch = 0;
  } else {
    all_delays = 0;
    iter_this_buffer = (prod_it - edge.prod_alpha) % edge.prod_beta;
    buffer_epoch = (prod_it - edge.prod_alpha) / edge.prod_beta + 1;
  }

  auto first_byte = all_delays + iter_this_buffer * edge.prod_rate;
  auto last_byte = all_delays + (iter_this_buffer + 1) * edge.prod_rate - 1;

  int64_t cons_lb, cons_ub;

  if (buffer_epoch == 0) {
    cons_lb = (first_byte - edge.remaining_delay) / edge.cons_rate;
    cons_ub = (last_byte - edge.remaining_delay) / edge.cons_rate;
  } else {
    auto offset = edge.cons_alpha + (buffer_epoch - 1) * edge.cons_beta;
    cons_lb = (first_byte) / edge.cons_rate + offset;
    cons_ub = (last_byte) / edge.cons_rate + offset;
  }

  Bounds rv = {cons_lb, cons_ub};

#ifdef IARA_RUNTIME_DEBUG
  fprintf(stderr, "prod to cons: mapping %ld:%ld to %ld:{%ld,%ld}\n",
          edge.prod_id, prod_it, edge.cons_id, rv.first, rv.last);
  fflush(stderr);
#endif

  return rv;
}

void iara_runtime_dealloc(int64_t iter, int64_t edge_id, uint8_t **buffer) {
  free(buffer[0]);

  iara_deallocd_buffers--;
  if (iara_deallocd_buffers == 0) {
    iara_runtime_graph_iteration_finished = true;
  }
};

void free_pointer_buffer(uint8_t **buffers) { free(buffers); }

// assumes not alloc or free
uint8_t *get_root_from_written_offset(int64_t prod_iteration, EdgeData &edge,
                                      uint8_t *written_offset) {
  if (prod_iteration < edge.prod_alpha) {
    return written_offset - edge.prod_rate * prod_iteration - edge.this_delay -
           edge.remaining_delay;
  }
  prod_iteration -= edge.prod_alpha;
  prod_iteration %= edge.prod_beta;
  return written_offset - edge.prod_rate * prod_iteration;
};

// Given an edge ID and a range of iteration numbers of the producer node,
// schedules the appropriate executions of the consumer node. Assumes the
// given range fits within the same buffer. The callback function is called
// with the accumulated offsets to be passed as arguments to the kernel
// function when an iteration has all its dependencies resolved.
void iara_runtime_processDependency(int64_t prod_iteration, int64_t edge_id,
                                    uint8_t *written_offset);

void trigger_firing(NodeData const &cons_node, int64_t cons_it,
                    uint8_t **offsets) {
  {
#ifdef IARA_RUNTIME_DEBUG
    fprintf(stderr, "running node (%ld)\n",
            int64_t(&cons_node - iara_runtime_static_data.nodes)),
        cons_it;
    fflush(stderr);
#endif

    if (cons_node.func_call == nullptr) { // dealloc

      return;
    }

    cons_node.func_call(cons_it, offsets);
    for (int i = 0, e = cons_node.output_port_count; i < e; i++) {
      {
        iara_runtime_processDependency(cons_it, cons_node.output_edges[i],
                                       offsets[i]);
      }
    }
    free_pointer_buffer(offsets);
  }
}

void semaphore(const NodeData &cons_node, const EdgeData &edge, int64_t cons_id,
               int64_t cons_it, int64_t bytes_deducted,
               int64_t input_index /* -1 if no need to set an argument */,
               uint8_t *offset) {

  // try to satisfy the dependence
  auto &cons_data = iara_runtime_dependency_dicts[edge.cons_id];

  // Only the first and last dependencies of a consumer need to change the
  // maps, so having a shared lock is enough for most dependencies.
  cons_data.mutex.lock_shared();
  auto iter = cons_data.counters.find(edge.cons_id);
  if (iter == cons_data.counters.end()) {
    // First time seeing this iteration; lock mutably to create atomic
    // counter and buffer
    do {
      cons_data.mutex.unlock_shared();
      if (cons_data.mutex.try_lock()) {
        auto [new_iter, inserted] = cons_data.counters.emplace(
            edge.cons_id, IterationDependencyCounter{});
        new_iter->second.bytes_remaining = getTotalBytes(edge, cons_it);

#ifdef IARA_RUNTIME_DEBUG
        fprintf(stderr,
                "First time running iteration %ld:%ld. Expected bytes: %ld\n",
                cons_id, cons_it, new_iter->second.bytes_remaining);
        fflush(stderr);
#endif

        new_iter->second.arguments = (uint8_t **)malloc(
            (size_t)cons_node.input_port_count * sizeof(uint8_t *));
        iter = new_iter;
        cons_data.mutex.unlock();
        cons_data.mutex.lock_shared();
        break;
      }
      cons_data.mutex.lock_shared();
      iter = cons_data.counters.find(cons_it);
    } while (iter == cons_data.counters.end());
  }

  auto &[_, runtime_data] = *iter;

  if (input_index != -1) {
    runtime_data.arguments[input_index] = offset;
  }

  auto before_sub = std::atomic_fetch_sub(
      (std::atomic_int *)&runtime_data.bytes_remaining, bytes_deducted);

#ifdef IARA_RUNTIME_DEBUG
  fprintf(stderr, "%ld:%ld = %d/%ld\n", cons_id, cons_it,
          runtime_data.bytes_remaining, getTotalBytes(edge, cons_it));
  fflush(stderr);
#endif

  if (before_sub - bytes_deducted == 0) {

#ifdef IARA_RUNTIME_DEBUG
    fprintf(stderr, "Done with iteration %ld:%ld\n", cons_id, cons_it);
    fflush(stderr);
#endif

    auto offsets = runtime_data.arguments;

    int priority = (edge.isDealloc() or edge.isAlloc()) ? 0 : 1;

    // #pragma omp task nowait priority(priority)
    { trigger_firing(cons_node, cons_it, offsets); }

    // #pragma omp task nowait priority(priority)
    {
      cons_data.mutex.unlock_shared();
      cons_data.mutex.lock();
      cons_data.counters.erase(iter);
      cons_data.mutex.unlock();
    }
  } else {
    cons_data.mutex.unlock_shared();
  }
}

void process_alloc_dependency(int64_t prod_iteration, int64_t edge_id,
                              uint8_t *buffer_root) {
  // #pragma omp task nowait
  {

    EdgeData *edge_data = iara_runtime_static_data.edges;
    EdgeData &edge = edge_data[edge_id];

    NodeData *node_data = iara_runtime_static_data.nodes;
    NodeData &cons_node = node_data[edge.cons_id];

    assert(edge.isAlloc());
    assert(buffer_root != NULL);
    int64_t buffer_size = (prod_iteration == 0)
                              ? edge.buffer_size_with_delays
                              : edge.buffer_size_without_delays;

#ifdef IARA_RUNTIME_DEBUG
    fprintf(
        stderr,
        "Processing alloc edge %ld, iteration %ld, cons_id %ld, buffer_root "
        "%lx, buffer size %ld\n",
        edge_id, prod_iteration, edge_data[edge_id].cons_id,
        (uint64_t)buffer_root, buffer_size);
    fflush(stderr);
#endif

    Bounds cons_bounds = [&]() -> Bounds {
      if (prod_iteration == 0) { // first allocation (with delays)
        return {edge.this_delay / edge.cons_rate, edge.cons_alpha - 1};
      } else { // second and later allocations (without delays)
        auto first = edge.cons_alpha + (prod_iteration - 1) * edge.cons_beta;
        return {first, first + edge.cons_beta - 1};
      }
    }();

    for (int i = 0; cons_bounds.first + i <= cons_bounds.last; i++) {
      semaphore(cons_node, edge, edge.cons_id, cons_bounds.first + i,
                edge.cons_rate, edge.cons_input_port_index,
                buffer_root + i * edge.cons_rate);
    }
  }
}

void process_delay_copy_back(int64_t prod_iteration, int64_t edge_id,
                             uint8_t *written_offset) {
  auto &edge = iara_runtime_static_data.edges[edge_id];
  auto &prod = iara_runtime_static_data.nodes[edge.prod_id];

  auto buffer_size = (prod_iteration > 0) ? edge.buffer_size_with_delays
                                          : edge.buffer_size_without_delays;

  auto current_buffer_offset = 0;
  if (prod_iteration > 0) {
    current_buffer_offset += edge.buffer_size_with_delays;
  }
  current_buffer_offset +=
      edge.buffer_size_without_delays * (prod_iteration - 1);

  auto dealloc_bounds =
      Bounds{current_buffer_offset, current_buffer_offset + buffer_size - 1};

  auto delay_copyback_bounds =
      Bounds(prod.total_firings * edge.prod_rate - edge.total_delay_size);
  delay_copyback_bounds.last =
      delay_copyback_bounds.first + edge.total_delay_size - 1;

  auto overlap = dealloc_bounds.clip(delay_copyback_bounds);
  if (overlap.size() > 0) {
    auto offset = overlap.first - delay_copyback_bounds.first;
    memcpy(edge.delay_buffer + offset, buffer[0] + offset, overlap.size());
  }
}

void process_dealloc_dependency(int64_t prod_iteration, int64_t edge_id,
                                uint8_t *written_offset) {
  // #pragma omp task nowait
  {
    EdgeData *edge_data = iara_runtime_static_data.edges;
    EdgeData &edge = edge_data[edge_id];

    NodeData *node_data = iara_runtime_static_data.nodes;
    NodeData &cons_node = node_data[edge.cons_id];

    assert(edge.isDealloc());

#ifdef IARA_RUNTIME_DEBUG
    fprintf(stderr,
            "Processing dealloc edge %ld, iteration %ld, cons_id %ld, "
            "buffer_root %lx\n",
            edge_id, prod_iteration, edge_data[edge_id].cons_id,
            (uint64_t)written_offset);
    fflush(stderr);
#endif

    assert(edge.prod_rate != 0);

    int64_t cons_it;
    bool is_first_of_buffer = false;
    int64_t expected_bytes;

    if (prod_iteration < edge.prod_alpha) {
      cons_it = 0;
      is_first_of_buffer = prod_iteration == 0;
      expected_bytes = edge.buffer_size_with_delays;
    } else {
      auto beta_prod_firings_so_far = prod_iteration - edge.prod_alpha;
      is_first_of_buffer = beta_prod_firings_so_far % edge.prod_beta == 0;
      cons_it = beta_prod_firings_so_far / edge.prod_beta + 1;
      expected_bytes = edge.buffer_size_without_delays;
    }

    if (is_first_of_buffer) {
      semaphore(cons_node, edge, edge.cons_id, cons_it, edge.prod_rate, 0,
                written_offset);
    } else {
      semaphore(cons_node, edge, edge.cons_id, cons_it, edge.prod_rate, -1,
                nullptr);
    }
  }
}

void process_conventional_dependency(int64_t prod_iteration, int64_t edge_id,
                                     uint8_t *written_offset) {
  // #pragma omp task nowait
  {
    EdgeData *edge_data = iara_runtime_static_data.edges;
    EdgeData &edge = edge_data[edge_id];

    NodeData *node_data = iara_runtime_static_data.nodes;
    NodeData &cons_node = node_data[edge.cons_id];

    uint8_t *buffer_root =
        get_root_from_written_offset(prod_iteration, edge, written_offset);
    assert(buffer_root != NULL);

#ifdef IARA_RUNTIME_DEBUG
    fprintf(stderr,
            "Processing conventional edge %ld, iteration %ld, cons_id %ld, "
            "buffer_root %lx\n",
            edge_id, prod_iteration, edge_data[edge_id].cons_id,
            (uint64_t)buffer_root);
    fflush(stderr);
#endif

    assert(edge.prod_rate != 0);
    // assert(not(edge.this_delay == 0 and (edge.prod_rate == edge.cons_rate))
    // &&
    //        "This edge does not need synchronization");

    Bounds cons_bounds = prod_to_cons(edge, {prod_iteration});

#ifdef IARA_RUNTIME_DEBUG
    fprintf(stderr, "prod_to_cons(%ld:%ld) -> %ld:(%ld, %ld)\n", edge_id,
            prod_iteration, edge.cons_id, cons_bounds.first, cons_bounds.last);
    fflush(stderr);
#endif

    assert(get_buffer_offset_cons(edge, cons_bounds.first).first ==
               get_buffer_offset_cons(edge, cons_bounds.last).first &&
           "This range of iterations of the producer node do not write into "
           "the same buffer.");

    for (auto cons_it = cons_bounds.first; cons_it <= cons_bounds.last;
         ++cons_it) {

      auto [buffer_epoch_prod, prod_memory_bounds] =
          get_buffer_offset_prod(edge, prod_iteration);

      auto [buffer_epoch_cons, cons_memory_bounds] =
          get_buffer_offset_cons(edge, cons_it);

      auto overlap = prod_memory_bounds.clip(cons_memory_bounds);

      auto provided_bytes = overlap.size();

      assert(provided_bytes > 0);
      assert(provided_bytes <= edge.prod_rate);

      if (overlap.first == cons_memory_bounds.first) {
        semaphore(cons_node, edge, edge.cons_id, cons_it, provided_bytes,
                  edge.cons_input_port_index,
                  buffer_root + cons_memory_bounds.first);
      } else {
        semaphore(cons_node, edge, edge.cons_id, cons_it, provided_bytes, -1,
                  nullptr);
      }
    }
  }
}

void iara_runtime_processDependency(int64_t prod_iteration, int64_t edge_id,
                                    uint8_t *written_offset) {
  auto edge = iara_runtime_static_data.edges[edge_id];
  assert(!edge.isAlloc() && "Alloc nodes don't have dependencies");

  if (edge.isDealloc()) {
    process_dealloc_dependency(prod_iteration, edge_id, written_offset);
    return;
  }

  process_conventional_dependency(prod_iteration, edge_id, written_offset);
}

void iara_runtime_copy_delays(int64_t node_id, uint8_t *buffer) {
  auto &node = iara_runtime_static_data.nodes[node_id];
  auto &edge = iara_runtime_static_data.edges[node.output_edges[0]];
  memcpy(buffer, node.init_buffer, edge.this_delay + edge.remaining_delay);
}

void iara_runtime_schedule_delays(int64_t prod_id, int64_t input_index,
                                  uint8_t *buffer_base) {
  auto &prod_node = iara_runtime_static_data.nodes[prod_id];
  auto &edge =
      iara_runtime_static_data.edges[prod_node.output_edges[input_index]];

  if (edge.isDealloc())
    return;

  auto &cons_node = iara_runtime_static_data.nodes[edge.cons_id];
  auto remaining_edge_delay = edge.this_delay;
  int64_t cons_it = 0;
  while (remaining_edge_delay > 0) {
    auto provided_bytes = std::min(remaining_edge_delay, edge.cons_rate);
    semaphore(cons_node, edge, edge.cons_id, cons_it, provided_bytes,
              edge.cons_input_port_index, buffer_base);
    remaining_edge_delay -= edge.cons_rate;
    buffer_base += edge.cons_rate;
    cons_it++;
  }

  iara_runtime_schedule_delays(edge.cons_id, edge.cons_input_port_index,
                               buffer_base);
}

void iara_wait_graph_iteration() {
  iara_runtime_graph_iteration_finished.wait(false);
}

// Executes an alloc actor. Runs with priority 0 (allocations won't happen if
// there is processing to be done)
extern "C" void iara_runtime_alloc(int64_t node_id) {
#ifdef IARA_RUNTIME_DEBUG
  fprintf(stderr, "Allocating node %ld\n", node_id);
  fflush(stderr);
#endif

  auto &node = iara_runtime_static_data.nodes[node_id];
  auto &edge = iara_runtime_static_data.edges[node.output_edges[0]];

  for (auto i = 0; i < node.total_firings; ++i) {
    // #pragma omp task nowait priority(0)
    {
      uint8_t *buffer;
      if (i == 0) {
        buffer = (uint8_t *)malloc(edge.buffer_size_with_delays);
        // todo: optimize this
        iara_runtime_copy_delays(node_id, buffer);
        iara_runtime_schedule_delays(edge.cons_id, edge.cons_input_port_index,
                                     buffer);
      } else {
        buffer = (uint8_t *)malloc(edge.buffer_size_without_delays);
      }
      process_alloc_dependency(i, node.output_edges[0], buffer);
    }
  }
}

void debug_static_data() {
  for (int n = 0; n < iara_runtime_static_data.num_nodes; n++) {
    auto &node = iara_runtime_static_data.nodes[n];
    printf("Node %d:\n", n);
    for (int e = 0; e < node.output_port_count; e++) {
      printf("   Output edge %ld\n", node.output_edges[e]);
    }
    if (node.init_buffer) {
      auto &edge = iara_runtime_static_data.edges[node.output_edges[0]];
      auto delay_size = (edge.this_delay + edge.remaining_delay) / 4;
      printf("   Delay initial values:\n   ");
      for (int i = 0; i < delay_size; i++) {
        printf("%d %s", ((int32_t *)(node.init_buffer))[i],
               (i + 1 < delay_size) ? ", " : "\n");
      }
    }
  }
}

extern "C" void iara_runtime_init() {

  debug_static_data();

  iara_deallocd_buffers = iara_runtime_static_data.num_buffers;
  iara_runtime_dependency_dicts =
      new DependencyDict[iara_runtime_static_data.num_nodes];

  for (int i = 0; i < iara_runtime_static_data.num_nodes; i++) {
    iara_runtime_dependency_dicts[i].mutex.lock_shared();
    iara_runtime_dependency_dicts[i].mutex.unlock_shared();
  }
}

/*     int64_t token_range_lower_bound = edge.this_delay +
   edge.remaining_delay
   + first_prod_iteration * edge.prod_rate; int64_t
   token_range_lower_bound_last_prod_firing = edge.this_delay +
   edge.remaining_delay + last_prod_iteration * edge.prod_rate;

    int64_t alpha_gen = 0;
    int64_t beta_gen = 0;

    if (first_prod_iteration >= edge.prod_alpha) {
      alpha_gen = 1;
      auto _div = div(first_prod_iteration - edge.prod_alpha,
   edge.prod_alpha); token_range_lower_bound = _div.rem * edge.prod_rate;
      beta_gen = _div.quot;
    }

    if (last_prod_iteration >= edge.prod_alpha) {
      alpha_gen = 1;
      auto _div = div(last_prod_iteration - edge.prod_alpha, edge.prod_alpha);
      token_range_lower_bound_last_prod_firing = _div.rem * edge.prod_rate;
      beta_gen = _div.quot;
    }

    int64_t token_range_upper_bound_inclusive =
        token_range_lower_bound_last_prod_firing + edge.prod_rate - 1;

    size_t this_buffer_it =
        alpha_gen * edge.cons_alpha + beta_gen * edge.cons_beta;

    size_t cons_it_lower_bound =
        this_buffer_it + token_range_lower_bound / edge.cons_rate;

    size_t cons_it_lower_bound_last_prod_firing =
        this_buffer_it +
        token_range_lower_bound_last_prod_firing / edge.cons_rate;

    size_t cons_it_upper_bound_inclusive =
        (cons_it_lower_bound_last_prod_firing + edge.cons_rate - 1) /
        edge.cons_rate;
 */