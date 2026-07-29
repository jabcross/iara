// StaticDataAccess implementations now inline in StaticDataAccess.h.
// This file exists only for out-of-line instantiation (rare, for debug builds).
#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"

namespace iara::runtime::virtualfifo {

VirtualFIFO_Node *getNode(iara::int_node idx) { return &iara_runtime_nodes[idx]; }

iara::int_node getNodeIndex(const VirtualFIFO_Node *n) {
  return static_cast<iara::int_node>(n - iara_runtime_nodes.data());
}

iara::int_node getNumNodes() { return static_cast<iara::int_node>(iara_runtime_nodes.size()); }

VirtualFIFO_Edge *getEdge(iara::int_edge idx) { return &iara_runtime_edges[idx]; }

iara::int_edge getEdgeIndex(const VirtualFIFO_Edge *e) {
  return static_cast<iara::int_edge>(e - iara_runtime_edges.data());
}

iara::int_edge getNumEdges() { return static_cast<iara::int_edge>(iara_runtime_edges.size()); }

VirtualFIFO_Node *getProducer(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes[e->codegen_info.producer_idx];
}

VirtualFIFO_Node *getConsumer(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes[e->codegen_info.consumer_idx];
}

VirtualFIFO_Node *getAllocNode(const VirtualFIFO_Edge *e) {
  return &iara_runtime_nodes[e->codegen_info.alloc_node_idx];
}

VirtualFIFO_Edge *getNextInChain(const VirtualFIFO_Edge *e) {
  return (e->runtime_info.cons_rate < 0)
             ? nullptr
             : const_cast<VirtualFIFO_Edge *>(e + 1);
}

void fireKernel(VirtualFIFO_Node *node, i64 seq, std::span<VirtualFIFO_Chunk> args) {
  iara_dispatch_kernel(node->codegen_info.kernel_id, seq, args);
}

} // namespace iara::runtime::virtualfifo
