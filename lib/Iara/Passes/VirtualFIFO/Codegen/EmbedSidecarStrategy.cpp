// EmbedSidecarStrategy.cpp — Strategy B for VirtualFIFO static data emission.
//
// Instead of emitting a giant schedule.mlir full of LLVM struct initializers,
// this strategy:
//   1. Builds a compact in-memory RuntimeBlob with u8/u16 fields (no pointers).
//   2. Writes the blob to ${BUILD_DIR}/static_data.bin.
//   3. Writes ${BUILD_DIR}/static_data.c that uses #embed to pull it in and
//      exposes the runtime arrays as extern C symbols.
//   4. Emits only extern declarations + iara_runtime_nodes/edges spans into
//      schedule.mlir (no struct initializers there).
//
// schedule.mlir is now small: dispatch glue, kernel wrappers, alloc/dealloc/
// broadcast chains, and extern decls. LLVM compile time drops by ~10x.
//
// Requires chain-contiguous edge ordering: the array is sorted so each inout
// chain occupies consecutive slots. End-of-chain detection: cons_rate < 0
// (dealloc edge) — no next_in_chain pointer field needed.

#include "Iara/Dialect/Node.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/Codegen/StaticDataEmitStrategy.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <span>
#include <string>
#include <vector>

#include "Iara/Passes/Common/Codegen/AsValue.h"
#include "Iara/Passes/Common/Codegen/Codegen.h"
#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Internal.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"

using namespace mlir;
using namespace mlir::LLVM;
using namespace iara::passes::virtualfifo::codegen;
using namespace iara::passes::virtualfifo::sdf;
using namespace iara::util::range;
using namespace iara::dialect;
using namespace iara::passes::common::codegen;

static_assert(sizeof(VirtualFIFO_Node) == 32,
              "VirtualFIFO_Node layout changed; update EmbedSidecarStrategy");
static_assert(sizeof(VirtualFIFO_Edge) == 112,
              "VirtualFIFO_Edge layout changed; update EmbedSidecarStrategy");

namespace {

// ---- helpers ----------------------------------------------------------------

static std::string getBuildDir() {
  const char *dir = std::getenv("PATH_TO_TEST_BUILD_DIR");
  return dir ? dir : ".";
}

static std::string getPath(const char *filename) {
  return getBuildDir() + "/" + filename;
}

// Return chains sorted chain-contiguous: for each alloc-node, walk
// next_edge links to emit the full chain before starting the next one.
// Within a chain, the last edge has cons_rate < 0 (dealloc convention).
static std::vector<EdgeCodegenData *>
sortEdgesChainContiguous(std::span<EdgeCodegenData> edges) {
  // Collect chain start edges (alloc → first real edge).
  // alloc nodes have exactly one output edge (the first chain edge).
  llvm::DenseSet<EdgeCodegenData *> visited;
  std::vector<EdgeCodegenData *> result;
  result.reserve(edges.size());

  // Find chain starts: edges whose producer is an alloc node.
  llvm::SmallVector<EdgeCodegenData *> starts;
  for (auto &e : edges) {
    if (e.producer && e.producer->node_op.isAlloc())
      starts.push_back(&e);
  }

  // Walk each chain in order.
  for (auto *start : starts) {
    auto *cur = start;
    while (cur && !visited.count(cur)) {
      visited.insert(cur);
      result.push_back(cur);
      cur = cur->next_edge;
    }
  }

  // Logic and borrow edges belong to no inout chain; they are appended
  // separately by the caller (grouped per producer). Every other edge must have
  // been reached.
  for (auto &e : edges) {
    if (!visited.count(&e) && !isLogicEdge(e.edge_op) &&
        !isBorrowEdge(e.edge_op)) {
      assert(false && "Edge not reached during chain-contiguous sort");
    }
  }

  return result;
}

// ---- emit MLIR extern decls + runtime span globals --------------------------

static void emitExterns(ModuleOp module,
                        OpBuilder mod_builder,
                        size_t num_nodes,
                        size_t num_edges,
                        size_t num_fifo_indices,
                        size_t num_delay_bytes) {
  auto loc = module.getLoc();
  auto *ctx = module.getContext();
  auto ptr_type = LLVMPointerType::get(ctx);
  auto i64_type = IntegerType::get(ctx, 64);
  auto span_type = getSpanType(ctx);

  // Helper: emit an extern global.
  auto emitExternGlobal = [&](StringRef name, Type type) {
    if (module.lookupSymbol(name))
      return;
    auto g = mod_builder.create<GlobalOp>(loc,
                                          type,
                                          /*isConstant=*/true,
                                          Linkage::External,
                                          name,
                                          Attribute{},
                                          /*alignment=*/8);
    g.setVisibility(mlir::SymbolTable::Visibility::Public);
  };

  // Runtime arrays (from static_data.c).
  auto node_arr_type = LLVMArrayType::get(IntegerType::get(ctx, 8),
                                          num_nodes * sizeof(VirtualFIFO_Node));
  emitExternGlobal("iara_runtime_data__node_infos", node_arr_type);

  auto edge_arr_type = LLVMArrayType::get(IntegerType::get(ctx, 8),
                                          num_edges * sizeof(VirtualFIFO_Edge));
  emitExternGlobal("iara_runtime_data__edge_infos", edge_arr_type);

  if (num_fifo_indices > 0) {
    auto fi_type =
        LLVMArrayType::get(IntegerType::get(ctx, 16), num_fifo_indices);
    emitExternGlobal("iara_runtime_node_input_fifos_flat", fi_type);
  }

  if (num_delay_bytes > 0) {
    auto delay_type =
        LLVMArrayType::get(IntegerType::get(ctx, 8), num_delay_bytes);
    emitExternGlobal("iara_runtime_edge_delays_flat", delay_type);
  }

  // iara_runtime_nodes/edges spans — defined in static_data.c, only
  // declared here for the LLVM module to reference them.
  {
    auto *ctx = module.getContext();
    auto span_type = getSpanType(ctx);

    auto makeExternSpan = [&](StringRef name) {
      if (module.lookupSymbol(name))
        return;
      auto g = mod_builder.create<GlobalOp>(
          loc, span_type, /*isConstant=*/false, Linkage::External,
          name, Attribute{}, /*alignment=*/8);
      g.setVisibility(mlir::SymbolTable::Visibility::Public);
    };
    makeExternSpan("iara_runtime_nodes");
    makeExternSpan("iara_runtime_edges");
  }
}

// ---- the strategy -----------------------------------------------------------

struct EmbedSidecarEmitter {
  ModuleOp module;
  OpBuilder module_builder;
  std::span<NodeCodegenData> node_pairs;
  std::span<EdgeCodegenData> edge_pairs;

  void emit() {
    size_t num_nodes = node_pairs.size();
    size_t num_edges = edge_pairs.size();

    assert(num_nodes <= std::numeric_limits<iara::int_node>::max() &&
           "int_node overflow: widen iara::int_node to u32");
    assert(num_edges <= std::numeric_limits<iara::int_edge>::max() &&
           "int_edge overflow: widen iara::int_edge to u32");

    // Sort edges chain-contiguous.
    auto sorted_edges = sortEdgesChainContiguous(edge_pairs);

    // Append logic (control-only) output edges, grouped per producer node so
    // each node's logic outputs occupy a contiguous range. They live after all
    // dealloc-terminated chains, so they never disturb the implicit e+1
    // chain-successor walk.
    std::vector<std::pair<iara::int_edge, u8>> node_logic_out(num_nodes, {0, 0});
    for (size_t i = 0; i < num_nodes; i++) {
      auto &nd = node_pairs[i];
      if (nd.logic_outputs.empty())
        continue;
      node_logic_out[i].first =
          static_cast<iara::int_edge>(sorted_edges.size());
      node_logic_out[i].second = static_cast<u8>(nd.logic_outputs.size());
      for (auto *le : nd.logic_outputs)
        sorted_edges.push_back(le);
    }

    // Remap edge indices after sorting.
    llvm::DenseMap<EdgeCodegenData *, iara::int_edge> edge_idx;
    for (size_t i = 0; i < sorted_edges.size(); i++)
      edge_idx[sorted_edges[i]] = static_cast<iara::int_edge>(i);

    // Build fifo-index flat array and node-side fifo storage.
    // Inline path: input_count <= 2; indirect path: spills to flat array.
    std::vector<iara::int_edge> fifo_flat;

    struct NodeFifoInfo {
      u8 flags;
      iara::int_edge inline_inputs[2];
      iara::int_edge indirect_start;
      iara::int_edge indirect_count;
    };
    std::vector<NodeFifoInfo> node_fifo(num_nodes);

    for (size_t i = 0; i < num_nodes; i++) {
      auto &nd = node_pairs[i];
      Node n(nd.node_op);
      size_t count = nd.inputs.size();
      u8 flags = 0;
      if (n.needsPriming())
        flags |= IARA_NODE_NEEDS_PRIMING;
      // All-read-only broadcast: fire() dispatches to fireBroadcast(); its
      // borrow reader edges are stored in the logic_out range.
      if (nd.node_op->hasAttr("broadcast_borrow"))
        flags |= IARA_NODE_IS_BROADCAST;

      if (count <= 2) {
        flags |= IARA_NODE_INPUTS_INLINE;
        node_fifo[i].flags = flags;
        node_fifo[i].inline_inputs[0] =
            (count > 0) ? edge_idx[nd.inputs[0]]
                        : (nd.node_op.isAlloc() && !nd.outputs.empty()
                               ? edge_idx[nd.outputs[0]]
                               : 0);
        node_fifo[i].inline_inputs[1] =
            (count > 1) ? edge_idx[nd.inputs[1]] : 0;
      } else {
        node_fifo[i].flags = flags;
        node_fifo[i].indirect_start =
            static_cast<iara::int_edge>(fifo_flat.size());
        node_fifo[i].indirect_count = static_cast<iara::int_edge>(count);
        for (auto *e : nd.inputs)
          fifo_flat.push_back(edge_idx[e]);
      }
    }

    // Build edge delay flat array.
    std::vector<unsigned char> delay_flat;
    struct EdgeDelayInfo {
      uint32_t start;
    };
    std::vector<EdgeDelayInfo> edge_delay(num_edges, {0});

    for (size_t i = 0; i < sorted_edges.size(); i++) {
      auto *ed = sorted_edges[i];
      Edge e(ed->edge_op);
      i64 delay_size = e.delaySize();
      if (delay_size > 0) {
        edge_delay[i].start = static_cast<uint32_t>(delay_flat.size());
        // The delay bytes are in the MLIR attr "delay" on the edge op.
        if (auto delay_attr = llvm::dyn_cast_or_null<DenseArrayAttr>(
                ed->edge_op["delay"].get())) {
          auto bytes = delay_attr.getRawData();
          delay_flat.insert(delay_flat.end(), bytes.begin(), bytes.end());
        } else {
          // Zero-fill if not available.
          delay_flat.insert(delay_flat.end(), delay_size, 0);
        }
      }
    }

    // ---- Build the blob ----

    // Node array.
    std::vector<VirtualFIFO_Node> node_blob(num_nodes);
    for (size_t i = 0; i < num_nodes; i++) {
      auto &nd = node_pairs[i];
      Node n(nd.node_op);
      auto &dst = node_blob[i];
      memset(&dst, 0, sizeof(dst));

      dst.runtime_info.arg_bytes = n.argBytes();
      dst.runtime_info.total_iter_firings =
          static_cast<uint32_t>(n.totalIterFirings());
      dst.runtime_info.num_args = static_cast<iara::int_edge>(n.numArgs());
      assert(n.logicInBytes() <= 255 &&
             "logic_in_bytes exceeds u8; widen the field (see Embed header)");
      dst.runtime_info.logic_in_bytes =
          static_cast<uint8_t>(n.logicInBytes());
      dst.runtime_info.flags = node_fifo[i].flags;
      // sema_variant: zero-initialized (calloc semantics from memset above).

      dst.codegen_info.kernel_id = nd.kernel_id;
      dst.codegen_info.logic_out_start = node_logic_out[i].first;
      dst.codegen_info.logic_out_count = node_logic_out[i].second;
      if (node_fifo[i].flags & IARA_NODE_INPUTS_INLINE) {
        dst.codegen_info.input_fifos.inline_inputs[0] =
            node_fifo[i].inline_inputs[0];
        dst.codegen_info.input_fifos.inline_inputs[1] =
            node_fifo[i].inline_inputs[1];
      } else {
        dst.codegen_info.input_fifos.indirect.start =
            node_fifo[i].indirect_start;
        dst.codegen_info.input_fifos.indirect.count =
            node_fifo[i].indirect_count;
      }
    }

    // Edge array (chain-contiguous order).
    std::vector<VirtualFIFO_Edge> edge_blob(num_edges);
    for (size_t i = 0; i < sorted_edges.size(); i++) {
      auto *ed = sorted_edges[i];
      Edge e(ed->edge_op);
      auto &dst = edge_blob[i];
      memset(&dst, 0, sizeof(dst));

      // RuntimeInfo (formerly StaticInfo).
      dst.runtime_info.local_index = e.localIndex();
      dst.runtime_info.prod_rate = e.prodRate();
      dst.runtime_info.cons_rate = e.consRate();
      dst.runtime_info.cons_arg_idx = e.consArgIdx();
      dst.runtime_info.delay_offset = e.delayOffset();
      dst.runtime_info.delay_size = e.delaySize();
      dst.runtime_info.block_size_with_delays = e.blockSizeWithDelays();
      dst.runtime_info.block_size_no_delays = e.blockSizeNoDelays();
      dst.runtime_info.prod_alpha = e.prodAlpha();
      dst.runtime_info.prod_beta = e.prodBeta();
      dst.runtime_info.cons_alpha = e.consAlpha();
      dst.runtime_info.cons_beta = e.consBeta();

      // CodegenInfo: u16 indices.
      dst.codegen_info.consumer_idx =
          ed->consumer ? static_cast<iara::int_node>(ed->consumer->index) : 0;
      dst.codegen_info.producer_idx =
          ed->producer ? static_cast<iara::int_node>(ed->producer->index) : 0;
      dst.codegen_info.alloc_node_idx =
          ed->alloc_node ? static_cast<iara::int_node>(ed->alloc_node->index)
                         : 0;
      dst.codegen_info.delay_start = edge_delay[i].start;
    }

    // ---- Write static_data.bin ----

    std::string bin_path = getPath("static_data.bin");
    std::string c_path = getPath("static_data.c");
    std::string h_path = getPath("static_data.h");

    // 8-byte zero prefix avoids Clang #embed encoding-detection false positive
    // (the first node's arg_bytes may start with 0xFE 0xFF = UTF-16 BE BOM).
    static constexpr size_t kHeaderSize = 8;

    {
      FILE *f = fopen(bin_path.c_str(), "wb");
      assert(f && "failed to open static_data.bin for writing");
      uint64_t header = 0;
      fwrite(&header, sizeof(header), 1, f);
      fwrite(node_blob.data(), sizeof(VirtualFIFO_Node), num_nodes, f);
      fwrite(edge_blob.data(), sizeof(VirtualFIFO_Edge), num_edges, f);
      if (!fifo_flat.empty())
        fwrite(fifo_flat.data(), sizeof(iara::int_edge), fifo_flat.size(), f);
      if (!delay_flat.empty())
        fwrite(delay_flat.data(), 1, delay_flat.size(), f);
      fclose(f);
    }

    // ---- Write static_data.c ----

    size_t node_offset = kHeaderSize;
    size_t edge_offset = node_offset + num_nodes * sizeof(VirtualFIFO_Node);
    size_t fifo_offset = edge_offset + num_edges * sizeof(VirtualFIFO_Edge);
    size_t delay_offset =
        fifo_offset + fifo_flat.size() * sizeof(iara::int_edge);

    {
      FILE *f = fopen(c_path.c_str(), "w");
      assert(f && "failed to open static_data.c for writing");

      fprintf(f, "// Auto-generated by EmbedSidecarStrategy — DO NOT EDIT\n");
      fprintf(f, "#include <stdint.h>\n");
      fprintf(f, "#include <stddef.h>\n");
      fprintf(f, "#include <string.h>\n\n");

      fprintf(f, "// Raw blob embedding all runtime static data.\n");
      fprintf(f, "// Not const: node/edge fields (e.g. semaphore pointers)\n");
      fprintf(f, "// are patched at runtime init.\n");
      fprintf(f, "alignas(8) static unsigned char _iara_blob[] = {\n");
      fprintf(f, "#embed \"static_data.bin\"\n");
      fprintf(f, "};\n\n");

      fprintf(f, "#define _IARA_NODE_SIZE %zu\n", sizeof(VirtualFIFO_Node));
      fprintf(f, "#define _IARA_EDGE_SIZE %zu\n", sizeof(VirtualFIFO_Edge));
      fprintf(f, "#define _IARA_NUM_NODES %zu\n", num_nodes);
      fprintf(f, "#define _IARA_NUM_EDGES %zu\n\n", num_edges);

      fprintf(f, "// Node/edge arrays exposed as extern symbols.\n");
      fprintf(f, "const void *const iara_runtime_data__node_infos =\n");
      fprintf(f, "    _iara_blob + %zu;\n", node_offset);
      fprintf(f, "const void *const iara_runtime_data__edge_infos =\n");
      fprintf(f, "    _iara_blob + %zu;\n\n", edge_offset);

      if (!fifo_flat.empty()) {
        fprintf(f,
                "const uint16_t *const iara_runtime_node_input_fifos_flat =\n");
        fprintf(
            f, "    (const uint16_t *)(_iara_blob + %zu);\n\n", fifo_offset);
      } else {
        fprintf(f,
                "const uint16_t *const iara_runtime_node_input_fifos_flat = "
                "0;\n\n");
      }
      if (!delay_flat.empty()) {
        fprintf(f,
                "const unsigned char *const iara_runtime_edge_delays_flat =\n");
        fprintf(f, "    _iara_blob + %zu;\n\n", delay_offset);
      } else {
        fprintf(f,
                "const unsigned char *const iara_runtime_edge_delays_flat = "
                "0;\n\n");
      }

      fprintf(f, "// Span<Node> and Span<Edge> for the runtime.\n");
      fprintf(f, "struct _IaraSpan { const void *data; size_t size; };\n");
      fprintf(f, "struct _IaraSpan iara_runtime_nodes = {\n");
      fprintf(f, "    _iara_blob + %zu, %zu };\n", node_offset, num_nodes);
      fprintf(f, "struct _IaraSpan iara_runtime_edges = {\n");
      fprintf(f, "    _iara_blob + %zu, %zu };\n\n", edge_offset, num_edges);

      fclose(f);
    }

    // ---- Emit extern decls into schedule.mlir ----
    emitExterns(module,
                module_builder,
                num_nodes,
                num_edges,
                fifo_flat.size(),
                delay_flat.size());
  }
};

struct EmbedSidecarStrategy : public StaticDataEmitStrategy {
  void emit(ModuleOp module,
            OpBuilder module_builder,
            std::span<NodeCodegenData> nodes,
            std::span<EdgeCodegenData> edges) override {
    EmbedSidecarEmitter{module, module_builder, nodes, edges}.emit();
  }
};

} // anonymous namespace

namespace iara::passes::virtualfifo::codegen {

std::unique_ptr<StaticDataEmitStrategy> makeEmbedSidecarStrategy() {
  return std::make_unique<EmbedSidecarStrategy>();
}

} // namespace iara::passes::virtualfifo::codegen