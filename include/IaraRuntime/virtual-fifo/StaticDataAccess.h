#ifndef IARA_RUNTIME_VIRTUAL_FIFO_STATIC_DATA_ACCESS_H
#define IARA_RUNTIME_VIRTUAL_FIFO_STATIC_DATA_ACCESS_H

#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include <span>

extern "C" {
extern std::span<VirtualFIFO_Node> iara_runtime_nodes;
extern std::span<VirtualFIFO_Edge> iara_runtime_edges;

// Flat index arrays and dispatch fn emitted by EmbedSidecarStrategy.
// Declared as pointers (matching the C-side `const T *const` globals)
// because the data lives in the static _iara_blob via #embed.
extern const iara::int_edge *const iara_runtime_node_input_fifos_flat;
extern const unsigned char  *const iara_runtime_edge_delays_flat;
void iara_dispatch_kernel(u8 kernel_id, i64 seq, std::span<VirtualFIFO_Chunk> args);
}

namespace iara::runtime::virtualfifo {

// ---- table accessors ----

inline VirtualFIFO_Node *getNode(u32 idx) { return &iara_runtime_nodes.data()[idx]; }
inline u32 getNodeIndex(const VirtualFIFO_Node *n) { return static_cast<u32>(n - iara_runtime_nodes.data()); }
inline u32 getNumNodes()  { return static_cast<u32>(iara_runtime_nodes.size()); }

inline VirtualFIFO_Edge *getEdge(u32 idx) { return &iara_runtime_edges.data()[idx]; }
inline u32 getEdgeIndex(const VirtualFIFO_Edge *e) { return static_cast<u32>(e - iara_runtime_edges.data()); }
inline u32 getNumEdges()  { return static_cast<u32>(iara_runtime_edges.size()); }

// ---- edge cross-ref accessors ----

inline VirtualFIFO_Node *getProducer(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes.data()[e->codegen_info.producer_idx];
}
inline VirtualFIFO_Node *getConsumer(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes.data()[e->codegen_info.consumer_idx];
}
inline VirtualFIFO_Node *getAllocNode(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes.data()[e->codegen_info.alloc_node_idx];
}
// Chain-contiguous layout: next = e+1 unless cons_rate < 0 (end of chain).
inline VirtualFIFO_Edge *getNextInChain(const VirtualFIFO_Edge *e) {
  return (e->runtime_info.cons_rate < 0)
             ? nullptr
             : const_cast<VirtualFIFO_Edge *>(e + 1);
}

// ---- kernel dispatch ----

inline void fireKernel(VirtualFIFO_Node *node, i64 seq, std::span<VirtualFIFO_Chunk> args) {
  iara_dispatch_kernel(node->codegen_info.kernel_id, seq, args);
}

} // namespace iara::runtime::virtualfifo

#endif // IARA_RUNTIME_VIRTUAL_FIFO_STATIC_DATA_ACCESS_H
