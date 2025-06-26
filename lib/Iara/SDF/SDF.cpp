#include "Iara/Dialect/IaraOps.h"
#include "Iara/SDF/Analysis.h"
#include "Iara/SDF/SDF.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/rational.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cmath>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <numeric>
#include <queue>

namespace iara::sdf

{
using namespace iara::util::mlir;
using namespace iara::util::range;

enum class Direction { Forward, Backward };

bool isDeallocEdge(EdgeOp edge) { return edge.getConsumerNode().isDealloc(); }

Vec<EdgeOp> getInoutChain(EdgeOp edge) {
  Vec<EdgeOp> rv;
  auto first = findFirstEdgeOfChain(edge);
  EdgeOp iter = first;
  while (iter) {
    rv.push_back(iter);
    iter = followInoutChainForwards(iter);
  }
  return rv;
}

NodeOp findFirstNodeOfChain(EdgeOp edge) {
  return findFirstEdgeOfChain(edge).getProducerNode();
}

Vec<Vec<EdgeOp>> getInoutChains(ActorOp actor) {
  Vec<Vec<EdgeOp>> chains;
  for (auto edge : actor.getOps<EdgeOp>()) {
    // Only once per inout chain.
    if (iara::followInoutChainBackwards(edge))
      continue;
    chains.push_back(getInoutChain(edge));
  }
  return chains;
}

Vec<InoutPair> getInoutPairs(NodeOp node) {
  Vec<InoutPair> rv;
  for (auto [i, o] : llvm::zip_first(node.getInout(), node.getResults())) {
    rv.emplace_back(i, o);
  }
  return rv;
}

LogicalResult annotateNodeRanks(ActorOp actor, StaticAnalysisData &data);
LogicalResult annotateTotalFirings(ActorOp actor, StaticAnalysisData &data);

LogicalResult annotateNodeInfo(ActorOp actor, StaticAnalysisData &data) {
  auto nodes = actor.getOps<NodeOp>() | IntoVector();
  auto id_range = getNextPowerOf10(nodes.size() + 1);
  for (auto [i, node] : enumerate(nodes)) {
    auto &info = data.node_static_info[node];
    auto input_bytes = 0;
    auto inputs = node.getAllInputs();
    for (auto v : inputs) {
      input_bytes += getTypeSize(v);
    }

    info = {
        .id = (i64)(i + 1 + id_range),
        .input_bytes = input_bytes,
        .num_inputs = (i64)inputs.size(),
        .rank = -1,
        .total_iter_firings = -1,
        .needs_priming = 1,
    };
    node["id"] = info.id;
  }
  return success(annotateNodeRanks(actor, data).succeeded() &&
                 annotateTotalFirings(actor, data).succeeded());
} // namespace iara::sdf

LogicalResult annotateDelayInfo(ActorOp actor, StaticAnalysisData &data);

// Sets the ids of the nodes and edges, as well as the position of each edge in
// its inout chain.
LogicalResult annotateEdgeInfo(ActorOp actor, StaticAnalysisData &data) {
  auto nodes = actor.getOps<NodeOp>() | IntoVector();
  auto id_range = getNextPowerOf10(nodes.size() + 1);
  for (auto [i, edge] : llvm::enumerate(actor.getOps<EdgeOp>())) {
    auto &info = data.edge_static_info[edge];
    auto prod_info = data.node_static_info[edge.getProducerNode()];
    auto cons_info = data.node_static_info[edge.getConsumerNode()];
    auto id = prod_info.id * id_range * 10 + cons_info.id;
    // if first of chain

    info = VirtualFIFO_Edge::StaticInfo{
        .id = id,
        // To fill in after alloc and dealloc generation.
        .local_index = -1,
        .prod_rate = edge.getProdRate(),
        .cons_rate = edge.getConsRate(),
        .cons_arg_idx = edge->getUses().begin()->getOperandNumber(),
        // To fill in in the delay info calculation.
        .delay_offset = -1,
        .delay_size = -1,
        // To fill in in the buffer sizing calculator.
        .block_size_with_delays = -1,
        .block_size_no_delays = -1,
        .prod_alpha = -1,
        .prod_beta = -1,
        .cons_alpha = -1,
        .cons_beta = -1,
    };
    edge["id"] = info.id;
  }

  return annotateDelayInfo(actor, data);
}

bool isInout(EdgeOp edge) {
  return llvm::is_contained(getInoutPairs(edge.getProducerNode()) |
                                Map([](auto p) { return p.out; }),
                            edge.getIn()) &&
         llvm::is_contained(edge.getConsumerNode().getInout(), edge.getOut());
}

void breakInout(EdgeOp edge) {
  edge.getProducerNode().dump();
  edge.dump();
  edge.getConsumerNode().dump();
  llvm_unreachable("Todo: break inout.");
}

LogicalResult annotateNodeRanks(ActorOp actor, StaticAnalysisData &data) {
  // run bfs on graph to get rank

  auto getRank = [&](NodeOp node) -> std::optional<i64> {
    if (auto entry = data.node_static_info.find(node);
        entry != data.node_static_info.end()) {
      if (entry->second.rank != -1)
        return entry->second.rank;
    }
    return {};
  };
  auto setRank = [&](NodeOp node, i64 value) {
    data.node_static_info[node].rank = value;
  };

  std::queue<std::pair<Operation *, int>> bfs{};

  // Start from all sources.
  for (auto node : actor.getOps<NodeOp>()) {
    if (node.getAllInputs().size() > 0)
      continue;
    Operation *op = node;
    bfs.emplace(op, 1);
  }

  auto flood_fill = [&](Operation *op, int rank) {
    auto node = dyn_cast<NodeOp>(op);
    if (node and getRank(node)) {
      return;
    }
    if (node)
      setRank(node, rank);
    // for (auto operand : op->getOperands()) {
    //   bfs.push({operand.getDefiningOp(), rank - 1});
    // }
    for (auto user : op->getUsers()) {
      bfs.push({user, rank + 1});
    }
  };

  while (!bfs.empty()) {
    auto [op, rank] = bfs.front();
    bfs.pop();
    flood_fill(op, rank);
  }

  for (auto edge : actor.getOps<EdgeOp>()) {
    if (data.node_static_info[edge.getConsumerNode()].rank <=
        data.node_static_info[edge.getProducerNode()].rank) {
      if (isInout(edge)) {
        breakInout(edge);
      }
      // todo: implement buffer
    }
  }
  return success();
}

// An edge is backwards if it creates a cycle with a bfs starting on the
// sources.
// TODO: check for appropriate delay size
template <class T> using IL = std::initializer_list<T>;
LogicalResult annotateDelayInfo(ActorOp actor, StaticAnalysisData &data) {

  for (auto edge : actor.getOps<EdgeOp>()) {

    // rate info

    auto &edge_info = data.edge_static_info[edge];
    edge_info.prod_rate = edge.getProdRate();
    edge_info.cons_rate = edge.getConsRate();
    edge_info.cons_arg_idx =
        edge.getResult().getUses().begin()->getOperandNumber();
    if (auto attr = edge->getAttr("delay")) {
      if (auto delay = dyn_cast_if_present<DenseArrayAttr>(attr)) {
        edge_info.delay_size =
            getTypeSize(delay.getElementType(), DataLayout::closest(edge)) *
            delay.getSize();
        continue;
      }
      llvm_unreachable("Unsupported type for delay");
      continue;
    } else {
      edge_info.delay_size = 0;
    }
  }

  if (analyseVirtualBufferSizes(actor, data).failed())
    return failure();

  // Fill out the delay offsets.
  for (auto chain : getInoutChains(actor)) {
    i64 offset = 0;
    for (auto edge : reverse(chain)) {
      auto &info = data.edge_static_info[edge];
      info.delay_offset = offset;
      offset += info.delay_size;
    }
  }

  return success();
} // namespace iara::sdf

SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> getNeighbors(NodeOp node) {
  SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> neighbors;
  auto inputs = node.getAllInputs();
  auto outputs = node.getAllOutputs();
  for (auto input : inputs) {
    auto edge = dyn_cast<EdgeOp>(input.getDefiningOp());
    assert(edge);
    auto other_node = edge.getProducerNode();
    neighbors.push_back({Direction::Backward, edge, other_node});
  }
  for (auto output : outputs) {
    auto edge = dyn_cast<EdgeOp>(*output.getUsers().begin());
    assert(edge);
    auto other_node = edge.getConsumerNode();
    neighbors.push_back({Direction::Forward, edge, other_node});
  }
  return neighbors;
}

LogicalResult annotateTotalFirings(ActorOp actor, StaticAnalysisData &data) {

  using util::Rational;

  if (actor.isKernelDeclaration())
    return success();
  assert(actor.isFlat());
  DenseMap<NodeOp, Rational> total_firings;
  // start walk
  SmallVector<NodeOp> stack, alloc_nodes, dealloc_nodes;

  for (auto node : actor.getOps<NodeOp>()) {
    stack.push_back(node);
    break;
  }

  SmallVector<i64> denominators = {1};

  total_firings[stack.back()] = {1};
  while (not stack.empty()) {
    auto node = stack.back();
    stack.pop_back();
    if (node.isAlloc() or node.isDealloc()) {
      continue;
    }
    for (auto [direction, edge, neighbor] : getNeighbors(node)) {
      Rational flow_ratio = edge.getFlowRatio().normalized();
      if (direction == Direction::Backward) {
        flow_ratio = flow_ratio.reciprocal();
      }
      auto neighbor_firings = (total_firings[node] * flow_ratio).normalized();
      if (total_firings.contains(neighbor)) {
        if (total_firings[neighbor] != neighbor_firings) {
          node->emitError(
              "Graph is not admissible (inconsistent iteration numbers ")
              << llvm::formatv(
                     "{0} and {1}", total_firings[neighbor], neighbor_firings)
              << "\n";
          // node->dump();
          // neighbor->dump();
          return failure();
        }
        continue;
      }
      total_firings[neighbor] = neighbor_firings;
      denominators.push_back(neighbor_firings.denom);
      stack.push_back(neighbor);
    }
  }

  // All iteration numbers must be integers. Multiply all by lcm of
  // denominators.
  i64 mult = std::reduce(denominators.begin(),
                         denominators.end(),
                         1,
                         [](auto a, auto b) { return std::lcm(a, b); });

  for (auto [node, firings] : total_firings) {
    firings = firings * mult;
    firings = firings.normalized();
    assert(firings.denom == 1);
    data.node_static_info[node].total_iter_firings = firings.num;
  }
  return success();
}

FailureOr<StaticAnalysisData> analyzeAndAnnotate(ActorOp actor) {
  StaticAnalysisData data;

  if (annotateNodeInfo(actor, data).succeeded() &&
      annotateEdgeInfo(actor, data).succeeded()) {
    return data;
  }
  return failure();
}

} // namespace iara::sdf
