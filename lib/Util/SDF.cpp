#include "Iara/IaraOps.h"
#include "Util/SDF.h"
#include "Util/rational.h"
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Support/LogicalResult.h>
#include <queue>

using namespace mlir::iara::mlir_util;

namespace mlir::iara::sdf

{

enum class Direction { Forward, Backward };

LogicalResult annotateNodeAndEdgeIds(ActorOp actor) {
  for (auto [i, n] : llvm::enumerate(actor.getOps<NodeOp>())) {
    n["node_id"] = i;
  }
  for (auto [i, e] : llvm::enumerate(actor.getOps<EdgeOp>())) {
    e["edge_id"] = i;
  }
  return success();
}

// An edge is backwards if it creates a cycle with a bfs starting on the
// sources.
// TODO: check for appropriate delay size
template <class T> using IL = std::initializer_list<T>;
LogicalResult annotateEdgeInfo(ActorOp actor) {
  // run bfs on graph to get rank

  auto getRank = [](Operation *op) -> std::optional<i64> {
    auto attr = op->getAttrOfType<IntegerAttr>("rank");
    if (attr)
      return {attr.getInt()};
    return {};
  };
  auto setRank = [](Operation *op, i64 value) {
    op->setAttr("rank", OpBuilder{op}.getI64IntegerAttr(value));
  };

  std::queue<std::pair<Operation *, int>> bfs{};

  Operation *start_node = *actor.getOps<NodeOp>().begin();

  bfs.push({start_node, 0});

  auto flood_fill = [&](Operation *op, int rank) {
    if (getRank(op)) {
      return;
    }
    setRank(op, rank);
    for (auto operand : op->getOperands()) {
      bfs.push({operand.getDefiningOp(), rank - 1});
    }
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

    // rate info

    edge["prod_rate"] = edge.getProdRate();
    edge["cons_rate"] = edge.getConsRate();

    // delay info
    if (getRank(edge.getConsumerNode()) <= getRank(edge.getProducerNode())) {
      edge->setAttr("backwards_edge", OpBuilder{edge}.getUnitAttr());
    }
    auto delay_attr = *edge["delay"];
    if (delay_attr == nullptr) {
      edge["delay_num_elems"] = 0;
    } else if (auto array =
                   llvm::dyn_cast_if_present<DenseArrayAttr>(delay_attr)) {
      edge["delay_num_elems"] = array.size();
    } else if (auto integer =
                   llvm::dyn_cast_if_present<IntegerAttr>(delay_attr)) {
      edge["delay_num_elems"] = integer.getInt();
    } else {
      llvm_unreachable("Unexpected delay attr type");
    }

    // size in bytes

    if (auto delay_elems = *edge["delay_num_elems"]) {
      edge["delay_size_bytes"] = (i64)edge["delay_num_elems"] *
                                 getTypeSize(getElementTypeOrSelf(edge.getIn()),
                                             DataLayout::closest(edge));
    } else {
      edge["delay_size_bytes"] = 0;
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

LogicalResult annotateTotalFirings(ActorOp actor) {

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
      if (direction == Direction::Forward) {
        flow_ratio = flow_ratio.reciprocal();
      }
      auto neighbor_firings = total_firings[node] * flow_ratio;
      if (total_firings.contains(neighbor)) {
        if (total_firings[neighbor] != neighbor_firings) {
          node->emitError(
              "Graph is not admissible (inconsistent iteration numbers ")
              << llvm::formatv("{0} and {1}", total_firings[neighbor],
                               neighbor_firings)
              << "\n";
          node->dump();
          neighbor->dump();
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

  i64 mult = std::reduce(denominators.begin(), denominators.end(), 1,
                         [](auto a, auto b) { return std::lcm(a, b); });

  for (auto [node, firings] : total_firings) {
    auto iteration_number = mult * firings.num;
    auto builder = OpBuilder(node);
    node->setAttr("total_firings", builder.getI64IntegerAttr(iteration_number));
  }

  return success();
}

LogicalResult analyzeAndAnnotate(ActorOp actor) {
  if (annotateNodeAndEdgeIds(actor).failed())
    return failure();
  if (annotateEdgeInfo(actor).failed())
    return failure();
  if (annotateTotalFirings(actor).failed()) {
    return failure();
  }
  return success();
}

} // namespace mlir::iara::sdf