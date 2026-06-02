#include "IaraRuntime/virtual-fifo/StaticDataAccess.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"

namespace iara::runtime::virtualfifo {

VirtualFIFO_Node *getNode(u32 idx) { return &iara_runtime_nodes[idx]; }

u32 getNodeIndex(const VirtualFIFO_Node *n) {
  return static_cast<u32>(n - iara_runtime_nodes.data());
}

u32 getNumNodes() { return static_cast<u32>(iara_runtime_nodes.size()); }

VirtualFIFO_Edge *getEdge(u32 idx) { return &iara_runtime_edges[idx]; }

u32 getEdgeIndex(const VirtualFIFO_Edge *e) {
  return static_cast<u32>(e - iara_runtime_edges.data());
}

u32 getNumEdges() { return static_cast<u32>(iara_runtime_edges.size()); }

VirtualFIFO_Node *getProducer(const VirtualFIFO_Edge *e) {
  return e->codegen_info.producer;
}

VirtualFIFO_Node *getConsumer(const VirtualFIFO_Edge *e) {
  return e->codegen_info.consumer;
}

VirtualFIFO_Node *getAllocNode(const VirtualFIFO_Edge *e) {
  return e->codegen_info.alloc_node;
}

VirtualFIFO_Edge *getNextInChain(const VirtualFIFO_Edge *e) {
  return e->codegen_info.next_in_chain;
}

std::span<VirtualFIFO_Edge *> getInputFifos(const VirtualFIFO_Node *n) {
  return n->codegen_info.input_fifos;
}

std::span<VirtualFIFO_Edge *> getOutputFifos(const VirtualFIFO_Node *n) {
  return n->codegen_info.output_fifos;
}

void fireKernel(VirtualFIFO_Node *node, i64 seq, std::span<VirtualFIFO_Chunk> args) {
  node->codegen_info.wrapper(seq, args);
}

} // namespace iara::runtime::virtualfifo
