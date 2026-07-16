#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/Node.h"
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
using namespace iara::dialect;

enum class Direction { Forward, Backward };

bool isDeallocEdge(EdgeOp edge) { return getConsumerNode(edge).isDealloc(); }

bool isLogicEdge(EdgeOp edge) {
  return Node::isLogicValue(edge.getOut()) || Node::isLogicValue(edge.getIn());
}

bool isBorrowEdge(EdgeOp edge) { return edge->hasAttr("borrow"); }

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
    // Logic and borrow edges have no buffer and belong to no inout chain.
    if (isLogicEdge(edge) || isBorrowEdge(edge))
      continue;
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
  for (auto [i, node] : enumerate(nodes)) {
    i64 arg_bytes = 0;
    i64 num_args = 0;
    i64 logic_in_bytes = 0;

    // Logic (control-only, `none`-typed) ports are normal SDF dependencies but
    // are NOT kernel arguments. A logic *input* still gates firing, so it counts
    // toward arg_bytes (the arrival threshold; getTypeSize(none)=1 token) but not
    // num_args (no kernel-arg slot). A logic *output* has no buffer and nothing
    // arrives for it, so it counts toward neither.
    for (auto pure_input : node.getIn()) {
      if (!Node::isLogicValue(pure_input)) {
        arg_bytes += getTypeSize(pure_input);
        num_args += 1;
      } else {
        // A logic input gates firing with `mult` tokens (1 for a 1:1 edge, or
        // this reader's firing multiplicity for a broadcast-join edge — see
        // annotateEdgeInfo). Add that many to the threshold, not one token.
        i64 mult = 1;
        if (auto e =
                llvm::dyn_cast_or_null<EdgeOp>(pure_input.getDefiningOp()))
          if (auto a = e->getAttrOfType<mlir::IntegerAttr>("logic_mult"))
            mult = a.getInt();
        arg_bytes += mult;
        logic_in_bytes += mult;
      }
    }
    for (auto inout : node.getInout()) {
      arg_bytes += getTypeSize(inout);
      num_args += 1;
    }
    for (auto pure_output : node.getPureOuts()) {
      if (Node::isLogicValue(pure_output))
        continue;
      // Borrow (read-only alias) outputs own no buffer and are produced by
      // aliasing in fireBroadcast(), not by an alloc the node waits on — exclude
      // them from the firing threshold and kernel-arg count (like logic outs).
      if (auto e = llvm::dyn_cast_or_null<EdgeOp>(
              *pure_output.getUsers().begin());
          e && isBorrowEdge(e))
        continue;
      arg_bytes += getTypeSize(pure_output);
      num_args += 1;
    }

    Node n(node);
    n.setArgBytes(arg_bytes);
    n.setNumArgs(num_args);
    n.setLogicInBytes(logic_in_bytes);
    n.setRank(-1);
    n.setTotalIterFirings(-1);
    n.setNeedsPriming(1);
  }
  return success(annotateNodeRanks(actor, data).succeeded() &&
                 annotateTotalFirings(actor, data).succeeded());
} // namespace iara::sdf

// Sets the position of each edge in its inout chain.
LogicalResult annotateEdgeInfo(ActorOp actor, StaticAnalysisData &data) {
  for (auto [i, edge] : llvm::enumerate(actor.getOps<EdgeOp>())) {
    Edge e(edge);
    // A logic edge carries no buffer and is skipped by the inout-chain analysis
    // that would otherwise set delay/block/alpha/beta. Producer emits 1 token
    // per firing (prod_rate 1); cons_rate is normally 1, but a broadcast-join
    // logic edge is MULTI-RATE (cons_rate = `logic_mult`, this reader's firings
    // per join firing), so the join gates on that many increments and fire()
    // maps the producer seq to the join seq. Not a kernel arg (cons_arg_idx=-1);
    // its token is delivered directly in fire() (never the byte-slicing path).
    if (isLogicEdge(edge)) {
      i64 mult = 1;
      if (auto a = edge->getAttrOfType<mlir::IntegerAttr>("logic_mult"))
        mult = a.getInt();
      e.setLocalIndex(0);
      e.setProdRate(1);
      e.setConsRate(mult);
      e.setConsArgIdx(-1);
      e.setDelayOffset(0);
      e.setDelaySize(0);
      e.setBlockSizeWithDelays(mult);
      e.setBlockSizeNoDelays(mult);
      e.setProdAlpha(0);
      e.setProdBeta(0);
      e.setConsAlpha(0);
      e.setConsBeta(0);
      continue;
    }
    // A borrow (read-only alias) edge carries data (a real kernel arg for the
    // consumer) but owns no buffer and belongs to no inout chain, so the chain
    // analysis never set its delay/block/alpha/beta. Rate it as a single-block
    // 1:1 data edge; fireBroadcast() pushes the whole aliased chunk.
    if (isBorrowEdge(edge)) {
      auto bytes = getProdRateBytes(edge);
      e.setLocalIndex(0);
      e.setProdRate(bytes);
      e.setConsRate(getConsRateBytes(edge));
      e.setConsArgIdx(edge->getUses().begin()->getOperandNumber());
      e.setDelayOffset(0);
      e.setDelaySize(0);
      e.setBlockSizeWithDelays(bytes);
      e.setBlockSizeNoDelays(bytes);
      e.setProdAlpha(0);
      e.setProdBeta(0);
      e.setConsAlpha(0);
      e.setConsBeta(0);
      continue;
    }
    // To fill in after alloc and dealloc generation.
    e.setLocalIndex(-1);
    e.setProdRate(getProdRateBytes(edge));
    e.setConsRate(getConsRateBytes(edge));
    e.setConsArgIdx(edge->getUses().begin()->getOperandNumber());

    // These should be already set.
    assert(e.delayOffset() != -1);
    assert(e.delaySize() != -1);
    assert(e.blockSizeWithDelays() != -1);
    assert(e.blockSizeNoDelays() != -1);
    assert(e.prodAlpha() != -1);
    assert(e.prodBeta() != -1);
    assert(e.consAlpha() != -1);
    assert(e.consBeta() != -1);
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
    i64 rank = Node(node).rank();
    if (rank != -1)
      return rank;
    return {};
  };
  auto setRank = [&](NodeOp node, i64 value) {
    Node(node).setRank(value);
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
    if (Node(getConsumerNode(edge)).rank() <=
        Node(getProducerNode(edge)).rank()) {
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
    Node(node).setTotalIterFirings(firings.num);
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
