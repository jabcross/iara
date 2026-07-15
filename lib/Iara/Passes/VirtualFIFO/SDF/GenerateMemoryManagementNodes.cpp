#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/Node.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::virtualfifo::sdf {

using namespace util::mlir;
using namespace util::range;
using namespace iara::dialect;

struct BufferSizeInfo {
  i64 total_size;
  i64 delays_only;
};

void populateAllocEdgeData(EdgeOp edge, StaticAnalysisData &data) {
  auto first_edge = followInoutChainForwards(edge);
  Edge alloc_e(edge);
  Edge first_e(first_edge);
  alloc_e.setProdRate(-1);
  alloc_e.setConsRate(first_e.prodRate());
  alloc_e.setProdAlpha(-1);
  alloc_e.setProdBeta(-1);
  alloc_e.setConsAlpha(first_e.prodAlpha());
  alloc_e.setConsBeta(first_e.prodBeta());
  alloc_e.setDelayOffset(first_e.delayOffset());
  alloc_e.setDelaySize(0);
  alloc_e.setBlockSizeWithDelays(first_e.blockSizeWithDelays());
  alloc_e.setBlockSizeNoDelays(first_e.blockSizeNoDelays());
}

EdgeOp createEdgeAdaptor(Value produced, Type consumed) {
  auto builder = OpBuilder(produced.getDefiningOp());
  builder.setInsertionPointAfterValue(produced);
  auto edge = CREATE(
      EdgeOp, builder, produced.getDefiningOp()->getLoc(), consumed, produced);
  return edge;
}

Value getPlaceholderValue(ModuleOp module) {
  return getIntConstant(module.getBody(), 0);
}

SmallVector<Value> createAllocations(ValueRange values) {
  SmallVector<Value> rv;
  for (auto val : values) {
    auto node = val.getDefiningOp<NodeOp>();
    auto builder = OpBuilder(node);
    // Create alloc node
    // tostring(parent(node));

    auto alloc_node =
        CREATE(NodeOp,
               builder,
               node->getLoc(),
               {UnrankedTensorType::get(
                   dyn_cast<RankedTensorType>(val.getType()).getElementType())},
               "iara_runtime_alloc",
               {},
               {},
               {});
    auto edge = createEdgeAdaptor(alloc_node.getOut().front(), val.getType());
    rv.push_back(edge.getOut());
  }
  return rv;
}

SmallVector<NodeOp> createDeallocations(ValueRange values) {
  SmallVector<NodeOp> rv;

  if (values.empty())
    return rv;

  // Replace with edge id once its generated
  // auto placeholder_value = getPlaceholderValue(
  //     values[0].getDefiningOp()->getParentOfType<ModuleOp>());
  for (auto original_val : values) {
    auto edge = createEdgeAdaptor(
        original_val,
        UnrankedTensorType::get(getElementTypeOrSelf(original_val.getType())));
    auto val = edge.getOut();
    auto builder = OpBuilder(edge);
    builder.setInsertionPointAfter(edge);
    // Create dealloc node
    auto dealloc_node = CREATE(NodeOp,
                               builder,
                               edge->getLoc(),
                               {},
                               "iara_runtime_dealloc",
                               {},
                               {val},
                               {});

    rv.push_back(dealloc_node);
  }
  return rv;
}

void annotateDeallocations(SmallVector<NodeOp> &dealloc_nodes,
                           StaticAnalysisData &data) {

  if (dealloc_nodes.empty())
    return;

  auto nodes =
      dealloc_nodes.front()->getParentOfType<ActorOp>().getOps<NodeOp>() |
      IntoVector();

  for (auto dealloc_node : dealloc_nodes) {
    auto dealloc_edge =
        cast<EdgeOp>(dealloc_node.getIn().front().getDefiningOp());
    auto last_node = getProducerNode(dealloc_edge);
    auto last_edge = followInoutChainBackwards(dealloc_edge);
    // Invariant: every dealloc edge has a preceding regular edge in its inout chain.
    assert(isa<EdgeOp>(last_edge) && "dealloc edge missing inout-chain predecessor");

    Node last_n(last_node);
    Edge last_e(last_edge);
    Node dealloc_n(dealloc_node);
    Edge dealloc_e(dealloc_edge);

    dealloc_n.setArgBytes(-3);
    dealloc_n.setNumArgs(1);
    dealloc_n.setRank(last_n.rank() + 2);
    dealloc_n.setTotalIterFirings(-3);
    dealloc_n.setNeedsPriming(0);

    // -1 = fill out later
    // -2 = N/A (alloc)
    // -3 = N/A (dealloc)

    dealloc_e.setLocalIndex(-1); // fill out later
    dealloc_e.setProdRate(last_e.consRate());
    dealloc_e.setConsRate(-3);
    dealloc_e.setConsArgIdx(1);
    dealloc_e.setDelayOffset(0);
    dealloc_e.setDelaySize(0);
    dealloc_e.setBlockSizeWithDelays(last_e.blockSizeWithDelays());
    dealloc_e.setBlockSizeNoDelays(last_e.blockSizeNoDelays());
    dealloc_e.setProdAlpha(last_e.consAlpha());
    dealloc_e.setProdBeta(last_e.consBeta());
    dealloc_e.setConsAlpha(-3);
    dealloc_e.setConsBeta(-3);
  }
}

void updateLocalIndices(EdgeOp edge, StaticAnalysisData &data, i64 start) {
  Edge(edge).setLocalIndex(start);
  if (auto next = followInoutChainForwards(edge)) {
    updateLocalIndices(next, data, start + 1);
  }
}

// Return how many firings of other nodes depend on this block (no delays)
// We assume that it's always going to be the same for any non-delay block.
i64 calculateFiringsPerBlock(NodeOp alloc_node, StaticAnalysisData &data) {
  auto alloc_edge = cast<EdgeOp>(*alloc_node->getUsers().begin());
  auto first_edge = followInoutChainForwards(alloc_edge);

  Edge first_e(first_edge);
  auto begin = first_e.blockSizeWithDelays();
  auto end = begin + first_e.blockSizeNoDelays();

  i64 dependent_firings_count = 0;

  auto chain = getInoutChain(alloc_edge);
  for (auto edge : chain) {
    if (isDeallocEdge(edge))
      continue;
    // Check if edge has been fully initialized with attributes
    if (!edge->hasAttr("local_index") || !edge->hasAttr("prod_rate") ||
        !edge->hasAttr("cons_rate") || !edge->hasAttr("cons_arg_idx") ||
        !edge->hasAttr("delay_offset") || !edge->hasAttr("delay_size") ||
        !edge->hasAttr("block_size_with_delays") || !edge->hasAttr("block_size_no_delays") ||
        !edge->hasAttr("prod_alpha") || !edge->hasAttr("prod_beta") ||
        !edge->hasAttr("cons_alpha") || !edge->hasAttr("cons_beta")) {
      // Edge attributes not yet initialized, skip this edge in calculation
      continue;
    }
    Edge e(edge);
    VirtualFIFO_Edge_RuntimeInfo si{};
    si.local_index = e.localIndex();
    si.prod_rate = e.prodRate();
    si.cons_rate = e.consRate();
    si.cons_arg_idx = e.consArgIdx();
    si.delay_offset = e.delayOffset();
    si.delay_size = e.delaySize();
    si.block_size_with_delays = e.blockSizeWithDelays();
    si.block_size_no_delays = e.blockSizeNoDelays();
    si.prod_alpha = e.prodAlpha();
    si.prod_beta = e.prodBeta();
    si.cons_alpha = e.consAlpha();
    si.cons_beta = e.consBeta();
    auto [bf, ef] = VirtualFIFO_Edge::getConsFiringsFromVirtualOffsetRange(
        si, begin, end);
    auto count = ef - bf;
    assert(count >= 0);
    dependent_firings_count += count;
  }
  return dependent_firings_count;
}

void annotateAllocations(SmallVector<Value> &vals, StaticAnalysisData &data) {

  if (vals.empty())
    return;

  auto nodes = vals.front()
                   .getDefiningOp()
                   ->getParentOfType<ActorOp>()
                   .getOps<NodeOp>() |
               IntoVector();

  for (auto val : vals) {
    auto alloc_edge = cast<EdgeOp>(val.getDefiningOp());
    auto first_node = cast<NodeOp>(*alloc_edge->getUsers().begin());
    auto alloc_node = (NodeOp)alloc_edge.getIn().getDefiningOp();
    populateAllocEdgeData(alloc_edge, data);
    auto first_edge = followInoutChainForwards(alloc_edge);

    Edge first_e(first_edge);
    Node first_n(first_node);
    Node alloc_n(alloc_node);
    Edge alloc_e(alloc_edge);

    auto operand_index =
        alloc_edge.getOut().getUses().begin()->getOperandNumber();

    alloc_n.setArgBytes(-2);
    alloc_n.setNumArgs(0);
    alloc_n.setRank(first_n.rank() - 2);
    alloc_n.setTotalIterFirings(calculateFiringsPerBlock(alloc_node, data));
    alloc_n.setNeedsPriming(0);

    // -1 = fill out later
    // -2 = N/A (alloc)
    // -3 = N/A (dealloc)

    alloc_e.setLocalIndex(-1); // fill out later
    alloc_e.setProdRate(-2);
    alloc_e.setConsRate(first_e.prodRate());
    alloc_e.setConsArgIdx(alloc_edge->getUses().begin()->getOperandNumber());
    alloc_e.setDelayOffset(first_e.delayOffset() + first_e.delaySize());
    alloc_e.setDelaySize(0);
    alloc_e.setBlockSizeWithDelays(first_e.blockSizeWithDelays());
    alloc_e.setBlockSizeNoDelays(first_e.blockSizeNoDelays());
    alloc_e.setProdAlpha(-2);
    alloc_e.setProdBeta(-2);
    alloc_e.setConsAlpha(first_e.prodAlpha());
    alloc_e.setConsBeta(first_e.prodBeta());

    updateLocalIndices(alloc_edge, data, 0);
  }
}

LogicalResult generateAllocsAndFrees(NodeOp old_node,
                                     StaticAnalysisData &data) {
  if (old_node.getIn().size() == 0 and old_node.getPureOuts().size() == 0)
    return success();
  auto builder = OpBuilder(old_node);

  // Partition pure inputs and pure outputs into data vs logic (`none`) ports.
  // Logic ports carry no buffer: a logic input stays a pure `in` (no
  // pass-through result, no dealloc); a logic output stays a pure result (no
  // alloc). Data ports keep the original threading — each data input becomes an
  // inout operand with a pass-through result feeding a dealloc, and each data
  // output is fed by a fresh alloc. With no logic ports this is identical to the
  // previous behavior.
  // Borrow (read-only alias) ports carry data but own no buffer: a borrow
  // pure-in stays a pure `in` (its consumer reads the aliased buffer — no
  // pass-through, no dealloc), and a borrow pure-out stays a pure result (the
  // broadcast aliases it in fireBroadcast — no alloc). Detected via the `borrow`
  // tag on the edge on the value's data side.
  auto isBorrowIn = [](Value v) {
    auto e = v.getDefiningOp<EdgeOp>();
    return e && isBorrowEdge(e);
  };
  auto isBorrowOut = [](Value v) {
    auto e = llvm::dyn_cast_or_null<EdgeOp>(*v.getUsers().begin());
    return e && isBorrowEdge(e);
  };

  SmallVector<Value> data_pure_ins, logic_pure_ins, borrow_pure_ins;
  for (auto v : old_node.getIn()) {
    if (Node::isLogicValue(v))
      logic_pure_ins.push_back(v);
    else if (isBorrowIn(v))
      borrow_pure_ins.push_back(v);
    else
      data_pure_ins.push_back(v);
  }

  SmallVector<Value> inout_ins = llvm::to_vector(old_node.getInout());
  auto inout_results =
      old_node.getResults().take_front(old_node.getInout().size());

  SmallVector<Value> data_pure_outs, logic_pure_outs, borrow_pure_outs;
  for (auto v : old_node.getPureOuts()) {
    if (Node::isLogicValue(v))
      logic_pure_outs.push_back(v);
    else if (isBorrowOut(v))
      borrow_pure_outs.push_back(v);
    else
      data_pure_outs.push_back(v);
  }

  auto alloc_inputs = createAllocations(data_pure_outs);

  // Inout operands, in the order their paired results appear.
  SmallVector<Value> inout_operands;
  inout_operands.append(data_pure_ins.begin(), data_pure_ins.end());
  inout_operands.append(inout_ins.begin(), inout_ins.end());
  inout_operands.append(alloc_inputs.begin(), alloc_inputs.end());

  // Result types: paired block (matches inout_operands order) then the unpaired
  // pure-out results (logic, then borrow).
  SmallVector<Type> new_result_types;
  for (auto v : data_pure_ins)
    new_result_types.push_back(v.getType());
  for (auto v : inout_results)
    new_result_types.push_back(v.getType());
  for (auto v : data_pure_outs)
    new_result_types.push_back(v.getType());
  for (auto v : logic_pure_outs)
    new_result_types.push_back(v.getType());
  for (auto v : borrow_pure_outs)
    new_result_types.push_back(v.getType());

  // Pure `in` operands: logic tokens plus borrow aliases (both gate/feed firing
  // without an inout pass-through).
  SmallVector<Value> pure_in_operands;
  pure_in_operands.append(logic_pure_ins.begin(), logic_pure_ins.end());
  pure_in_operands.append(borrow_pure_ins.begin(), borrow_pure_ins.end());

  auto new_node = CREATE(NodeOp,
                         builder,
                         old_node->getLoc(),
                         new_result_types,
                         old_node.getImpl(),
                         old_node.getParams(),
                         /*in=*/pure_in_operands,
                         /*inout=*/inout_operands);

  // Copy attributes from old_node to new_node
  Node old_n(old_node);
  Node new_n(new_node);
  new_n.setArgBytes(old_n.argBytes());
  new_n.setNumArgs(old_n.numArgs());
  new_n.setRank(old_n.rank());
  new_n.setTotalIterFirings(old_n.totalIterFirings());
  new_n.setNeedsPriming(old_n.needsPriming());

  new_node->setDiscardableAttrs(old_node->getDiscardableAttrDictionary());

  // Deallocs: one per data pure-in pass-through (the first block of results).
  auto passthrough_outs = new_node.getOut().take_front(data_pure_ins.size());
  auto new_dealloc_nodes = createDeallocations(passthrough_outs);

  // Rewire old results to their new positions (skip the leading pass-through
  // block, which is new).
  auto new_results = new_node.getResults();
  size_t base = data_pure_ins.size();
  for (auto [k, oldr] : llvm::enumerate(inout_results))
    oldr.replaceAllUsesWith(new_results[base + k]);
  base += inout_results.size();
  for (auto [k, oldr] : llvm::enumerate(data_pure_outs))
    oldr.replaceAllUsesWith(new_results[base + k]);
  base += data_pure_outs.size();
  for (auto [k, oldr] : llvm::enumerate(logic_pure_outs))
    oldr.replaceAllUsesWith(new_results[base + k]);
  base += logic_pure_outs.size();
  for (auto [k, oldr] : llvm::enumerate(borrow_pure_outs))
    oldr.replaceAllUsesWith(new_results[base + k]);

  old_node->erase();

  annotateDeallocations(new_dealloc_nodes, data);
  annotateAllocations(alloc_inputs, data);
  return success();
}

// Replaces nodes that have pure ins or outs with new ones and wire them to
// new alloc and dealloc nodes.
LogicalResult generateAllocsAndFrees(ActorOp actor, StaticAnalysisData &data) {
  for (auto old_node : llvm::to_vector(actor.getOps<NodeOp>())) {
    if (generateAllocsAndFrees(old_node, data).failed())
      return failure();
  }
  // Recompute alloc dependent-firing counts now that every inout chain is
  // fully formed (alloc -> producer -> ... -> consumer -> dealloc). The
  // per-node pass above annotates allocs inline while their downstream nodes
  // are still pure in/out, so getInoutChain truncates, and
  // calculateFiringsPerBlock undercounts the dependents. The prime() loop
  // pings the alloc for every kernel in the chain; with an undercount the
  // alloc semaphore takes the trivial always-fire path and re-allocates on
  // every ping — double-allocating blocks (BarrierTranspose SIGSEGV, feedback
  // chain null-arg crash in delay-hierarchy tests).
  for (auto node : actor.getOps<NodeOp>()) {
    if (!node.isAlloc())
      continue;
    Node(node).setTotalIterFirings(calculateFiringsPerBlock(node, data));
  }
  return success();
}
} // namespace iara::passes::virtualfifo::sdf
