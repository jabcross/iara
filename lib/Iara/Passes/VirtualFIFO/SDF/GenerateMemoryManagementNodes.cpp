#include "Iara/Dialect/IaraOps.h"
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
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::virtualfifo::sdf {

using namespace util::mlir;
using namespace util::range;

struct BufferSizeInfo {
  i64 total_size;
  i64 delays_only;
};

void populateAllocEdgeData(EdgeOp edge, StaticAnalysisData &data) {
  auto first_edge = followInoutChainForwards(edge);
  auto &alloc_edge_info = data.edge_static_info[edge];
  auto &first_edge_info = data.edge_static_info[first_edge];
  alloc_edge_info.prod_rate = -1;
  alloc_edge_info.cons_rate = first_edge_info.prod_rate;
  alloc_edge_info.prod_alpha = -1;
  alloc_edge_info.prod_beta = -1;
  alloc_edge_info.cons_alpha = first_edge_info.prod_alpha;
  alloc_edge_info.cons_beta = first_edge_info.prod_beta;
  alloc_edge_info.delay_offset = first_edge_info.delay_offset;
  alloc_edge_info.delay_size = 0;
  alloc_edge_info.block_size_with_delays =
      first_edge_info.block_size_with_delays;
  alloc_edge_info.block_size_no_delays = first_edge_info.block_size_no_delays;
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
    auto &last_node_info = data.node_static_info[last_node];
    auto last_edge = followInoutChainBackwards(dealloc_edge);
    auto &last_edge_info = data.edge_static_info[last_edge];
    auto &dealloc_node_info = data.node_static_info[dealloc_node];

    auto dealloc_node_id = [&]() {
      std::string id =
          llvm::formatv("40{0}0{1}",
                        last_node_info.id,
                        util::mlir::getResultIndex(dealloc_edge.getIn()) + 1);
      i64 rv;
      bool failed = StringRef(id).getAsInteger(10, rv);
      assert(!failed);
      return rv;
    }();

    dealloc_node_info = VirtualFIFO_Node::StaticInfo{
        .id = dealloc_node_id,
        .arg_bytes = -3,
        .num_args = 1,
        .rank = last_node_info.rank + 2,
        .total_iter_firings = -3,
        .needs_priming = 0,
    };

    dealloc_node["id"] = dealloc_node_id;

    auto dealloc_edge_id = [&]() {
      std::string id = llvm::formatv("2{0}", dealloc_node_id);
      i64 rv;
      bool failed = StringRef(id).getAsInteger(10, rv);
      assert(!failed);
      return rv;
    }();

    auto &dealloc_edge_info = data.edge_static_info[dealloc_edge];

    // -1 = fill out later
    // -2 = N/A (alloc)
    // -3 = N/A (dealloc)

    dealloc_edge_info = VirtualFIFO_Edge::StaticInfo{
        .id = dealloc_edge_id,
        .local_index = -1, // fill out later
        .prod_rate = last_edge_info.cons_rate,
        .cons_rate = -3,
        .cons_arg_idx = 1,
        .delay_offset = 0,
        .delay_size = 0,
        .block_size_with_delays = last_edge_info.block_size_with_delays,
        .block_size_no_delays = last_edge_info.block_size_no_delays,
        .prod_alpha = last_edge_info.cons_alpha,
        .prod_beta = last_edge_info.cons_beta,
        .cons_alpha = -3,
        .cons_beta = -3};

    dealloc_edge["id"] = dealloc_edge_id;
  }
}

void updateLocalIndices(EdgeOp edge, StaticAnalysisData &data, i64 start) {
  data.edge_static_info[edge].local_index = start;
  if (auto next = followInoutChainForwards(edge)) {
    updateLocalIndices(next, data, start + 1);
  }
}

// Return how many firings of other nodes depend on this block (no delays)
// We assume that it's always going to be the same for any non-delay block.
i64 calculateFiringsPerBlock(NodeOp alloc_node, StaticAnalysisData &data) {
  auto alloc_edge = cast<EdgeOp>(*alloc_node->getUsers().begin());
  auto first_edge = followInoutChainForwards(alloc_edge);

  auto first_edge_info = data.edge_static_info[first_edge];

  auto begin = first_edge_info.block_size_with_delays;
  auto end = begin + first_edge_info.block_size_no_delays;

  i64 dependent_firings_count = 0;

  auto chain = getInoutChain(alloc_edge);
  for (auto edge : chain) {
    if (isDeallocEdge(edge))
      continue;
    auto &edge_info = data.edge_static_info[edge];
    auto [bf, ef] = VirtualFIFO_Edge::getConsFiringsFromVirtualOffsetRange(
        edge_info, begin, end);
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
    auto &alloc_node_info = data.node_static_info[alloc_node];
    auto &first_edge_info = data.edge_static_info[first_edge];
    auto &first_node_info = data.node_static_info[first_node];

    auto operand_index =
        alloc_edge.getOut().getUses().begin()->getOperandNumber();

    auto alloc_node_id = [&]() {
      std::string id =
          llvm::formatv("30{0}0{1}", first_node_info.id, operand_index);
      i64 rv;
      bool failed = StringRef(id).getAsInteger(10, rv);
      assert(!failed);
      return rv;
    }();

    alloc_node_info = VirtualFIFO_Node::StaticInfo{
        .id = alloc_node_id,
        .arg_bytes = -2,
        .num_args = 0,
        .rank = first_node_info.rank - 2,
        .total_iter_firings = calculateFiringsPerBlock(alloc_node, data),
        .needs_priming = 0};

    alloc_node["id"] = alloc_node_id;

    auto &alloc_edge_info = data.edge_static_info[alloc_edge];

    auto alloc_edge_id = [&]() {
      std::string id = llvm::formatv("2{0}", alloc_node_id);
      i64 rv;
      bool failed = StringRef(id).getAsInteger(10, rv);
      assert(!failed);
      return rv;
    }();

    // -1 = fill out later
    // -2 = N/A (alloc)
    // -3 = N/A (dealloc)

    alloc_edge_info = VirtualFIFO_Edge::StaticInfo{
        .id = alloc_edge_id,
        .local_index = -1, // fill out later
        .prod_rate = -2,
        .cons_rate = first_edge_info.prod_rate,
        .cons_arg_idx = alloc_edge->getUses().begin()->getOperandNumber(),
        .delay_offset =
            first_edge_info.delay_offset + first_edge_info.delay_size,
        .delay_size = 0,
        .block_size_with_delays = first_edge_info.block_size_with_delays,
        .block_size_no_delays = first_edge_info.block_size_no_delays,
        .prod_alpha = -2,
        .prod_beta = -2,
        .cons_alpha = first_edge_info.prod_alpha,
        .cons_beta = first_edge_info.prod_beta};

    alloc_edge["id"] = alloc_node_id;

    updateLocalIndices(alloc_edge, data, 0);
  }
}

LogicalResult generateAllocsAndFrees(NodeOp old_node,
                                     StaticAnalysisData &data) {
  if (old_node.getIn().size() == 0 and old_node.getPureOuts().size() == 0)
    return success();
  auto builder = OpBuilder(old_node);

  SmallVector<Value> new_node_inputs = old_node.getAllInputs();
  auto alloc_inputs = createAllocations(old_node.getPureOuts());
  new_node_inputs.append(alloc_inputs);

  SmallVector<Type> new_result_types =
      llvm::to_vector(old_node.getIn().getTypes());
  new_result_types.append(to_vector(old_node->getResultTypes()));

  auto new_node = CREATE(NodeOp,
                         builder,
                         old_node->getLoc(),
                         new_result_types,
                         old_node.getImpl(),
                         old_node.getParams(),
                         {},
                         new_node_inputs);

  assert(new_node_inputs.size() == new_result_types.size());
  data.node_static_info[new_node] = data.node_static_info[old_node];

  new_node->setDiscardableAttrs(old_node->getDiscardableAttrDictionary());

  auto new_outs = new_node.getOut().take_front(old_node.getIn().size());
  auto existing_outs = new_node.getOut().drop_front(old_node.getIn().size());
  auto new_dealloc_nodes = createDeallocations(new_outs);

  for (auto [old, new_] : llvm::zip_equal(old_node.getOut(), existing_outs)) {
    old.replaceAllUsesWith(new_);
  }
  old_node->erase();
  data.node_static_info.erase(old_node);

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
  return success();
}
} // namespace iara::passes::virtualfifo::sdf
