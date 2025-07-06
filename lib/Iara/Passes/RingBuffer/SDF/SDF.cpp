#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/RingBuffer/SDF/SDF.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/rational.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cmath>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <numeric>
#include <queue>

namespace iara::passes::ringbuffer::sdf

{
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
        .num_ins = (i64)node.getIn().size(),
        .num_inouts = (i64)node.getInout().size(),
        .num_outs = (i64)node.getPureOuts().size(),
        .working_memory_size = -1,
        .rank = -1,
        .total_iter_firings = -1,
    };
    node["id"] = info.id;
  }
  return success(annotateNodeRanks(actor, data).succeeded() &&
                 annotateTotalFirings(actor, data).succeeded());
} // namespace iara::passes::ringbuffer::sdf

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

    auto token_type = getElementTypeOrSelf(edge.getOut().getType());
    auto token_align =
        DataLayout::closest(edge).getTypeABIAlignment(token_type);
    auto prod_rate_aligned = getProdRateBytes(edge);
    if (auto rem = prod_rate_aligned % token_align; rem != 0) {
      prod_rate_aligned = prod_rate_aligned - rem + token_align;
    }
    auto cons_rate_aligned = getConsRateBytes(edge);
    if (auto rem = cons_rate_aligned % token_align; rem != 0) {
      cons_rate_aligned = cons_rate_aligned - rem + token_align;
    }

    i64 delay_size = 0;

    if (auto attr = edge->getAttr("delay")) {
      if (auto delay = dyn_cast_if_present<DenseArrayAttr>(attr)) {
        delay_size = iara::util::mlir::getTypeSize(delay.getElementType(),
                                                   DataLayout::closest(edge)) *
                     delay.getSize();
      } else if (auto delay = dyn_cast_if_present<IntegerAttr>(attr)) {
        // zero init
        delay_size = delay.getInt();
      } else
        llvm_unreachable("Unsupported type for delay");
    }

    info = RingBuffer_Edge::StaticInfo{
        id,
        getProdRateBytes(edge),
        prod_rate_aligned,
        getConsRateBytes(edge),
        cons_rate_aligned,
        edge->getUses().begin()->getOperandNumber(),
        delay_size,
    };
    edge["id"] = info.id;
  }
  return success();
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
      // todo: implement buffer
    }
  }
  return success();
}

SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> getNeighbors(NodeOp node) {
  SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> neighbors;
  auto inputs = node.getAllInputs();
  auto outputs = node.getAllOutputs();
  for (auto input : inputs) {
    auto edge = dyn_cast<EdgeOp>(input.getDefiningOp());
    assert(edge);
    auto other_node = getProducerNode(edge);
    neighbors.push_back({Direction::Backward, edge, other_node});
  }
  for (auto output : outputs) {
    auto edge = dyn_cast<EdgeOp>(*output.getUsers().begin());
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

void annotateWorkingMemorySize(ActorOp actor, StaticAnalysisData &data) {
  for (auto node : actor.getOps<NodeOp>()) {
    i64 working_memory_size = 0;

    for (auto i : node.getIn()) {
      auto input_edge = cast<EdgeOp>(i.getDefiningOp());
      working_memory_size +=
          data.edge_static_info[input_edge].cons_rate_aligned;
    }
    for (auto i : node.getInout()) {
      auto input_edge = cast<EdgeOp>(i.getDefiningOp());
      working_memory_size +=
          data.edge_static_info[input_edge].cons_rate_aligned;
    }
    for (auto i : node.getPureOuts()) {
      auto output_edge = cast<EdgeOp>(*i.getUsers().begin());
      working_memory_size +=
          data.edge_static_info[output_edge].prod_rate_aligned;
    }

    data.node_static_info[node].working_memory_size = working_memory_size;
  }
}

FailureOr<StaticAnalysisData> analyzeAndAnnotate(ActorOp actor) {
  StaticAnalysisData data;

  if (annotateNodeInfo(actor, data).succeeded() &&
      annotateEdgeInfo(actor, data).succeeded()) {
    annotateWorkingMemorySize(actor, data);
    return data;
  }
  return failure();
}

} // namespace iara::passes::ringbuffer::sdf
