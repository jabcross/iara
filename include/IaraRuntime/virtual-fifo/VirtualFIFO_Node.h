#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include <cstddef>
#include <span>
#include <tuple>

#ifdef IARA_COMPILER
  #include "Iara/Passes/Common/Codegen/GetMLIRType.h"
  // #include <boost/describe.hpp>
#endif

struct VirtualFIFO_Edge; // forward decl

using KernelWrapper = void(i64 seq, std::span<VirtualFIFO_Chunk> args);

extern "C" {

struct VirtualFIFO_NormalSemaphore;
struct VirtualFIFO_AllocSemaphore;
union VirtualFIFO_Node_Semaphore {
  std::nullptr_t null;
  VirtualFIFO_NormalSemaphore *normal;
  VirtualFIFO_AllocSemaphore *alloc;
};

// Data from static analysis
struct VirtualFIFO_Node_StaticInfo {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Node_StaticInfo";

  using TupleType = std::tuple<i64, i64, i64, i64, i64, i64>;
  i64 id = -1;
  i64 arg_bytes = -1;
  i64 num_args = -1;
  i64 rank = -1;
  i64 total_iter_firings = -1; // For normal nodes, it is the number of times
                               // it fires in an iteration.
                               // For alloc nodes, it is the number of firings
                               // that depend on a (delay-less) block
  i64 needs_priming = 1; // for normal nodes: 0 if can run whenever the inputs
                         // are ready, 1 if needs to be scheduled by run_iter

  bool isAlloc() { return arg_bytes == -2; }
  bool isDealloc() { return arg_bytes == -3; }

  inline void dump() {
    fprintf(stderr, "Dumping nodeinfo {\n");
    fprintf(stderr, "id = %ld\n", id);
    fprintf(stderr, "input_bytes = %ld\n", arg_bytes);
    fprintf(stderr, "num_args = %ld\n", num_args);
    fprintf(stderr, "rank = %ld\n", rank);
    fprintf(stderr, "total_iter_firings = %ld\n", total_iter_firings);
    fprintf(stderr, "needs_priming = %ld\n", needs_priming);
  }
};

// Data that must be initialized by codegen
struct VirtualFIFO_Node_CodegenInfo {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Node_CodegenInfo";

  using TupleType = std::tuple<char *,
                               KernelWrapper *,
                               std::span<VirtualFIFO_Edge *>,
                               std::span<VirtualFIFO_Edge *>>;
  const char *name;
  KernelWrapper *wrapper;
  std::span<VirtualFIFO_Edge *> input_fifos;
  std::span<VirtualFIFO_Edge *> output_fifos;
};

// Data initialized by the runtime
struct VirtualFIFO_Node_RuntimeInfo {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Node_RuntimeInfo";

  using TupleType = std::tuple<VirtualFIFO_Node_Semaphore>;
  VirtualFIFO_Node_Semaphore sema_variant{nullptr};
};

struct VirtualFIFO_Node {
  static constexpr char STRUCT_NAME[] = "VirtualFIFO_Node";

  VirtualFIFO_Node_StaticInfo static_info;
  VirtualFIFO_Node_CodegenInfo codegen_info;
  VirtualFIFO_Node_RuntimeInfo runtime_info;

  // END WARNING

  // methods

  inline bool needs_priming() {
    return !static_info.isAlloc() && static_info.needs_priming;
  }

  void
  consume(i64 seq, VirtualFIFO_Chunk chunk, i64 arg_idx, i64 offset_partial);
  void dealloc(i64 current_buffer_size,
               i64 first_buffer_size,
               i64 next_buffer_sizes,
               VirtualFIFO_Chunk chunk);
  void init();

  // Cause this firing to execute.
  void prime(i64 seq);

  void fire(i64 seq, std::span<VirtualFIFO_Chunk>);

  void fireAlloc(i64 seq);

  void ensureAlloc(i64 firing);

  std::pair<i64, i64> getAllocDependentFirings(i64 iteration);

  void kickstart_alloc(i64 graph_iteration);
};

void iara_runtime_node_init(VirtualFIFO_Node *node);
}

#ifdef IARA_COMPILER

namespace iara::passes::common::codegen {

template <> struct GetMLIRType<VirtualFIFO_Node_Semaphore> {
  static mlir::Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};
// namespace iara::passes::common::codegen
} // namespace iara::passes::common::codegen

#endif

#endif // IARA_RUNTIME_SDF_NODE_H
