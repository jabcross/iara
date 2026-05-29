#ifndef IARA_RUNTIME_VIRTUAL_FIFO_STATIC_DATA_ACCESS_H
#define IARA_RUNTIME_VIRTUAL_FIFO_STATIC_DATA_ACCESS_H

#include "Iara/Util/CommonTypes.h"
#include <span>

struct VirtualFIFO_Node;
struct VirtualFIFO_Edge;

extern "C" {
extern std::span<VirtualFIFO_Node> iara_runtime_nodes;
extern std::span<VirtualFIFO_Edge> iara_runtime_edges;
}

namespace iara::runtime::virtualfifo {

VirtualFIFO_Node *getNode(u32 idx);
u32 getNodeIndex(const VirtualFIFO_Node *n);
u32 getNumNodes();

VirtualFIFO_Edge *getEdge(u32 idx);
u32 getEdgeIndex(const VirtualFIFO_Edge *e);
u32 getNumEdges();

VirtualFIFO_Node *getProducer(const VirtualFIFO_Edge *e);
VirtualFIFO_Node *getConsumer(const VirtualFIFO_Edge *e);
VirtualFIFO_Node *getAllocNode(const VirtualFIFO_Edge *e);
VirtualFIFO_Edge *getNextInChain(const VirtualFIFO_Edge *e);

std::span<VirtualFIFO_Edge *> getInputFifos(const VirtualFIFO_Node *n);
std::span<VirtualFIFO_Edge *> getOutputFifos(const VirtualFIFO_Node *n);

} // namespace iara::runtime::virtualfifo

#endif
