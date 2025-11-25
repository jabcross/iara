#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/VirtualFIFO/SDF/VirtualFIFOAnalysis.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/rational.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
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

namespace iara::passes::virtualfifo::sdf {
using namespace iara::util::mlir;
using namespace iara::util::range;

enum class Direction { Forward, Backward };

bool isDeallocEdge(EdgeOp edge) { return getConsumerNode(edge).isDealloc(); }

Vec<EdgeOp> getInoutChain(EdgeOp edge) {
  Vec<EdgeOp> rv;
  auto first = findFirstEdgeOfChain(edge);
  EdgeOp iter = first;
  while (iter) {
    rv.push_back(iter);
    iter = followInoutChainForwards(iter);
  }
  assert(rv.size() > 0);
  return rv;
}

NodeOp findFirstNodeOfChain(EdgeOp edge) {
  return getProducerNode(findFirstEdgeOfChain(edge));
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

LogicalResult annotateNodeRanks(ActorOp actor, StaticAnalysisData &data);
LogicalResult annotateTotalFirings(ActorOp actor, StaticAnalysisData &data);

LogicalResult annotateNodeInfo(ActorOp actor, StaticAnalysisData &data) {
  auto nodes = actor.getOps<NodeOp>() | IntoVector();
  auto id_range = getNextPowerOf10(nodes.size() + 1);
  for (auto [i, node] : enumerate(nodes)) {
    auto &info = data.node_static_info[node];

    i64 arg_bytes = 0;
    i64 num_args = 0;

    for (auto pure_input : node.getIn()) {
      arg_bytes += getTypeSize(pure_input);
      num_args += 1;
    }
    for (auto inout : node.getInout()) {
      arg_bytes += getTypeSize(inout);
      num_args += 1;
    }
    for (auto pure_output : node.getPureOuts()) {
      arg_bytes += getTypeSize(pure_output);
      num_args += 1;
    }

    info = {
        .id = (i64)(i + 1 + id_range),
        .arg_bytes = arg_bytes,
        .num_args = num_args,
        .rank = -1,
        .total_iter_firings = -1,
        .needs_priming = 1,
    };
    node["id"] = info.id;
  }
  return success(annotateNodeRanks(actor, data).succeeded() &&
                 annotateTotalFirings(actor, data).succeeded());
} // namespace iara::sdf

// Sets the ids of the nodes and edges, as well as the position of each edge in
// its inout chain.
LogicalResult annotateEdgeInfo(ActorOp actor, StaticAnalysisData &data) {
  auto nodes = actor.getOps<NodeOp>() | IntoVector();
  auto id_range = getNextPowerOf10(nodes.size() + 1);
  for (auto [i, edge] : llvm::enumerate(actor.getOps<EdgeOp>())) {
    auto &info = data.edge_static_info[edge];
    auto prod_info = data.node_static_info[getProducerNode(edge)];
    auto cons_info = data.node_static_info[getConsumerNode(edge)];
    auto id = prod_info.id * id_range * 10 + cons_info.id;
    // if first of chain

    info.id = id;
    // To fill in after alloc and dealloc generation.
    info.local_index = -1;
    info.prod_rate = getProdRateBytes(edge);
    info.cons_rate = getConsRateBytes(edge);
    info.cons_arg_idx = edge->getUses().begin()->getOperandNumber();

    // These should be already set.

    assert(info.delay_offset != -1);
    assert(info.delay_size != -1);
    assert(info.block_size_with_delays != -1);
    assert(info.block_size_no_delays != -1);
    assert(info.prod_alpha != -1);
    assert(info.prod_beta != -1);
    assert(info.cons_alpha != -1);
    assert(info.cons_beta != -1);

    edge["id"] = info.id;
  }

  return success();
}

bool isInout(EdgeOp edge) {
  return llvm::is_contained(getInoutPairs(getProducerNode(edge)) |
                                Map([](auto p) { return p.out; }),
                            edge.getIn()) &&
         llvm::is_contained(getConsumerNode(edge).getInout(), edge.getOut());
}

void breakInout(EdgeOp edge) {
  getProducerNode(edge).dump();
  edge.dump();
  getConsumerNode(edge).dump();
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
    if (data.node_static_info[getConsumerNode(edge)].rank <=
        data.node_static_info[getProducerNode(edge)].rank) {
      if (isInout(edge)) {
        breakInout(edge);
      }
      // todo: implement buffer
    }
  }
  return success();
}

SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> getNeighbors(NodeOp node) {
  SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> neighbors;
  auto inputs = node.getAllInputs();
  auto outputs = node.getAllOutputs();
  //  auto insize = inputs.size();
  // auto outsize = outputs.size();
  for (auto input : inputs) {
    auto edge = dyn_cast<EdgeOp>(input.getDefiningOp());
    assert(edge);
    auto other_node = getProducerNode(edge);
    neighbors.push_back({Direction::Backward, edge, other_node});
  }
  for (auto output : outputs) {
    auto users = output.getUsers() | IntoVector();
    assert(users.size() == 1);
    Operation *user = users[0];
    auto edge = dyn_cast<EdgeOp>(user);
    assert(edge);
    auto other_node = getConsumerNode(edge);
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
      Rational flow_ratio = getFlowRatio(edge).normalized();
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

  for (auto chain : getInoutChains(actor)) {
    if (analyzeVirtualInoutChain(chain, data).failed())
      return failure();
  }

  if (annotateNodeInfo(actor, data).succeeded() &&
      annotateEdgeInfo(actor, data).succeeded()) {
    return data;
  }

  // for (auto edge : actor.getOps<EdgeOp>()) {
  //   auto &info = data.edge_static_info[edge];
  //   edge.dump();
  // }

  return failure();
}

} // namespace iara::passes::virtualfifo::sdf
