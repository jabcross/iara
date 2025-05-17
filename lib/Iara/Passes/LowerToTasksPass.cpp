#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "Iara/Passes/LowerToTasksPass.h"
#include "Iara/Passes/Schedule/BufferSizeCalculator.h"
#include "IaraRuntime/IaraRuntime.h"
#include "Util/Canon.h"
#include "Util/MlirUtil.h"
#include "Util/OpCreateHelper.h"
#include "Util/RangeUtil.h"
#include "Util/ShellUtil.h"
#include "Util/rational.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include <any>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <istream>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/TypeSwitch.h>
#include <llvm/CodeGen/GlobalISel/Utils.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/MatrixBuilder.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/TypeSize.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <memory>
#include <mlir/Analysis/Presburger/MPInt.h>
#include <mlir/Analysis/Presburger/Matrix.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/DLTI/DLTI.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/OpenMP/OpenMPDialect.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributeInterfaces.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/Region.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Transforms/ViewOpGraph.h>
#include <numeric>
#include <optional>
#include <queue>
#include <ratio>
#include <sstream>
#include <streambuf>
#include <string>
#include <system_error>
#include <tuple>
#include <unistd.h>

using namespace mlir::iara::rangeutil;
using namespace mlir::iara::mlir_util;
using util::Rational;
template <class T> using Pair = std::pair<T, T>;
using mlir::presburger::IntMatrix;
using mlir::presburger::MPInt;

namespace mlir::iara::passes {

using Direction = LowerToTasksPass::Direction;

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
  llvm_unreachable("Unsupported operation");
}

NodeOp LowerToTasksPass::getMatchingDealloc(NodeOp alloc_node) {
  assert(alloc_node.isAlloc());
  auto edge = dyn_cast<EdgeOp>(*alloc_node->getUsers().begin());
  assert(edge);
  while (auto next_edge = followInoutEdgeForwards(edge)) {
    edge = next_edge;
  }
  auto dealloc_node = edge.getConsumerNode();
  assert(dealloc_node.isDealloc());
  return dealloc_node;
}

NodeOp LowerToTasksPass::getMatchingAlloc(NodeOp dealloc_node) {
  assert(dealloc_node.isDealloc());
  auto edge = followChainUntilPrevious<EdgeOp>(dealloc_node.getIn().front());
  assert(edge);
  while (auto prev_edge = followInoutEdgeBackwards(edge)) {
    edge = prev_edge;
  }
  auto alloc_node = edge.getProducerNode();
  assert(alloc_node.isAlloc());
  return alloc_node;
}

// LogicalResult annotateTotalFirings(ActorOp actor) {
//   if (actor.isKernelDeclaration())
//     return success();
//   assert(actor.isFlat());
//   DenseMap<NodeOp, util::Rational> total_firings;
//   // start walk
//   SmallVector<NodeOp> stack, alloc_nodes, dealloc_nodes;

//   for (auto node : actor.getOps<NodeOp>()) {
//     stack.push_back(node);
//     break;
//   }

//   SmallVector<i64> denominators = {1};

//   total_firings[stack.back()] = {1};
//   while (not stack.empty()) {
//     auto node = stack.back();
//     stack.pop_back();
//     if (node.isAlloc() or node.isDealloc()) {
//       continue;
//     }
//     for (auto [direction, edge, neighbor] : getNeighbors(node)) {
//       Rational flow_ratio = edge.getFlowRatio().normalized();
//       if (direction == Direction::Forward) {
//         flow_ratio = flow_ratio.reciprocal();
//       }
//       auto neighbor_firings = total_firings[node] * flow_ratio;
//       if (total_firings.contains(neighbor)) {
//         if (total_firings[neighbor] != neighbor_firings) {
//           node->emitError(
//               "Graph is not admissible (inconsistent iteration numbers ")
//               << llvm::formatv("{0} and {1}", total_firings[neighbor],
//                                neighbor_firings)
//               << "\n";
//           node->dump();
//           neighbor->dump();
//           return failure();
//         }
//         continue;
//       }
//       total_firings[neighbor] = neighbor_firings;
//       denominators.push_back(neighbor_firings.denom);
//       stack.push_back(neighbor);
//     }
//   }

//   // All iteration numbers must be integers. Multiply all by lcm of
//   // denominators.

//   i64 mult = std::reduce(denominators.begin(), denominators.end(), 1,
//                              [](auto a, auto b) { return std::lcm(a, b); });

//   for (auto [node, firings] : total_firings) {
//     auto iteration_number = mult * firings.num;
//     auto builder = OpBuilder(node);
//     node->setAttr("total_firings",
//     builder.getI64IntegerAttr(iteration_number));
//   }

//   return success();
// }

// func::FuncOp LowerToTasksPass::codegenBroadcastImpl(Value value, i64
// size) {
//   std::string name = llvm::formatv("iara_broadcast_{0}x{1}", size,
//                                    mlir_util::stringifyType(value.getType()));

//   // check if exists

//   if (auto existing = getOperation().lookupSymbol(name)) {
//     return dyn_cast<func::FuncOp>(existing);
//   }

//   auto module_builder = OpBuilder(getOperation());

//   module_builder.setInsertionPointToStart(getOperation().getBody());

//   SmallVector<Type> input_types, output_types;

//   for (auto i = 0; i < size; i++) {
//     input_types.push_back(
//         LLVM::LLVMPointerType::get(module_builder.getContext()));
//   }

//   auto impl =
//       CREATE(func::FuncOp, module_builder, value.getDefiningOp()->getLoc(),
//              name, module_builder.getFunctionType(input_types,
//              output_types));

//   auto body = impl.addEntryBlock();
//   auto impl_builder = OpBuilder(impl);
//   impl_builder.setInsertionPointToStart(body);

//   auto size_val = mlir_util::getIntConstant(&impl.getFunctionBody().front(),
//                                             (size_t)size *
//                                             getTypeSize(value));

//   auto src = body->getArgument(0);

//   for (auto i : body->getArguments().drop_front(1)) {
//     CREATE(LLVM::MemcpyOp, impl_builder, impl->getLoc(), i, src, size_val,
//            false);
//   }

//   CREATE(func::ReturnOp, impl_builder, impl->getLoc(), {});

//   return impl;
// }

// Inserts a broadcast node to copy given value. The first output is inout,
// the following outputs generate copies.
LogicalResult LowerToTasksPass::expandToBroadcast(OpResult &value) {
  OpBuilder builder(value.getOwner());
  builder.setInsertionPointAfter(value.getOwner());

  ValueRange in = {};
  ValueRange inout = {value};

  // SmallVector<Type> outputs;

  auto uses = value.getUses() | Pointers() | IntoVector();
  assert(uses.size() >= 2);
  auto output_types =
      uses | Map{[](auto &x) { return x->get().getType(); }} | IntoVector();

  auto key = std::make_pair(value.getType(), uses.size());
  auto impl = broadcast_impls.getOrInsertDefault(key);

  if (!impl) {
    impl = broadcast_impls[key] = codegenBroadcastImpl(value, uses.size());
  }

  assert(impl);

  auto broadcast_op = CREATE(NodeOp, builder, value.getOwner()->getLoc(),
                             output_types, impl.getSymName(), {}, in, inout);

  for (auto [new_value, operand] :
       llvm::zip_equal(broadcast_op.getOut(), uses)) {
    operand->set(new_value);
  }

  assert((value.getUses() | Pointers() | Count()) == 1);
  return success();
}

LogicalResult LowerToTasksPass::annotateIDs(ActorOp actor) {
  i64 edge_id_counter = 0;
  {
    for (auto [i, edge] : llvm::enumerate(actor.getOps<EdgeOp>())) {
      edge["edge_id"] = edge_id_counter;
    }
  }

  i64 node_id_counter = 0;
  for (auto node : actor.getOps<NodeOp>()) {
    node["node_id"] = node_id_counter;
    node_id_counter++;
    if (node.isDealloc()) {
      auto edge = followChainUntilPrevious<EdgeOp>(node.getIn()[0]);
      auto edge_id = (i64)edge["edge_id"];
      auto id_op = getIntConstant(edge->getBlock(), edge_id);
      node.getParamsMutable()[0].set(id_op);
    }
  }

  return success();
}

i64 calculateRemainingDelayBytes(EdgeOp edge) {
  i64 rv = 0;
  auto next = edge;
  while ((next = followInoutEdgeForwards(next))) {
    rv += (i64)next["delay_bytes"];
  }
  return rv;
}

i64 LowerToTasksPass::getConsPortIndex(EdgeOp edge) {
  return edge->getUses().begin()->getOperandNumber();
}

struct BufferSizeInfo {
  i64 total_size;
  i64 delays_only;
};

BufferSizeInfo getBufferSizeWithDelays(EdgeOp edge) {
  BufferSizeInfo rv;
  while (auto prev_edge = followInoutEdgeBackwards(edge)) {
    edge = prev_edge;
  }
  rv.delays_only = edge["delay_bytes"] + calculateRemainingDelayBytes(edge);
  rv.total_size =
      rv.delays_only + (i64)edge["prod_rate"] * (i64)edge["prod_alpha"];
  return rv;
}

i64 getBufferSizeWithoutDelays(EdgeOp edge) {
  while (auto prev_edge = followInoutEdgeBackwards(edge)) {
    edge = prev_edge;
  }
  return (i64)edge["prod_rate"] * (i64)edge["prod_beta"];
}

LogicalResult LowerToTasksPass::allocateContiguousBuffer(EdgeOp edge) {
  // find alpha and beta for each port

  if (edge->hasAttr("prod_alpha"))
    return success();
  while (auto previousInoutEdge = followInoutEdgeBackwards(edge)) {
    edge = previousInoutEdge;
  }

  SmallVector<i64> rates, delays;

  // auto getRepetitionNumber = [](Operation *op) {
  //   return op->getAttr("repetition_number").cast<IntegerAttr>().getInt();
  // };

  rates.push_back(edge.getProdRate());
  delays.push_back((i64)edge["delay_bytes"]);
  rates.push_back(edge.getConsRate());

  {
    auto next_edge = edge;
    while ((next_edge = followInoutEdgeForwards(next_edge))) {
      delays.push_back((i64)next_edge["delay_bytes"]);
      rates.push_back(next_edge.getConsRate());
    }
  }

  auto result = calculateBufferSize(rates, delays);

  if (failed(result)) {
    edge->emitError() << "Failed to calculate buffer size.";
    return failure();
  }

  auto [alpha, beta] = result.value();

  // auto has_delays = llvm::any_of(delays, [](i64 d) { return d > 0; });

  auto builder = OpBuilder(edge);

  // Populate the edge information
  {
    auto forward_edge = edge;
    int i = 0;
    while (forward_edge) {
      auto this_delay = (i64)forward_edge["delay_bytes"];
      auto remaining_delay = calculateRemainingDelayBytes(forward_edge);
      forward_edge["remaining_delay"] = remaining_delay;
      forward_edge["prod_rate"] = forward_edge.getProdRate();
      forward_edge["cons_rate"] = forward_edge.getConsRate();
      forward_edge["prod_alpha"] = alpha[i];
      forward_edge["prod_beta"] = beta[i];
      forward_edge["cons_alpha"] = alpha[i + 1];
      forward_edge["cons_beta"] = beta[i + 1];
      forward_edge = followInoutEdgeForwards(forward_edge);
      i++;
    }
  }

  return success();
}

LogicalResult LowerToTasksPass::allocateContiguousBuffers(ActorOp actor) {
  for (auto edge : actor.getOps<EdgeOp>()) {
    if (allocateContiguousBuffer(edge).failed()) {
      return failure();
    }
  }
  for (auto edge : actor.getOps<EdgeOp>()) {
    auto info = getBufferSizeWithDelays(edge);
    edge["buffer_size_with_delays"] = info.total_size;
    edge["buffer_size_without_delays"] = getBufferSizeWithoutDelays(edge);
    edge["total_delay_size"] = info.delays_only;
  }
  return success();
}

func::FuncOp codegenCopyImpl(Value input) {
  std::string name =
      llvm::formatv("iara_copy_{1}", mlir_util::stringifyType(input.getType()));

  auto module = input.getDefiningOp()->getParentOfType<ModuleOp>();

  // check if exists

  if (auto existing = module.lookupSymbol(name)) {
    return dyn_cast<func::FuncOp>(existing);
  }

  auto module_builder = OpBuilder(module);

  module_builder.setInsertionPointToStart(module.getBody());

  auto llvm_pointer_type = LLVM::LLVMPointerType::get(input.getContext());

  auto impl = CREATE(func::FuncOp, module_builder,
                     input.getDefiningOp()->getLoc(), name,
                     module_builder.getFunctionType(
                         {llvm_pointer_type, llvm_pointer_type}, {}));

  auto body = impl.addEntryBlock();
  auto impl_builder = OpBuilder(impl);
  impl_builder.setInsertionPointToStart(body);

  auto size_val = mlir_util::getIntConstant(&impl.getFunctionBody().front(),
                                            getTypeSize(input));

  CREATE(LLVM::MemcpyOp, impl_builder, impl->getLoc(), body->getArgument(1),
         body->getArgument(0), size_val, false);

  CREATE(func::ReturnOp, impl_builder, impl->getLoc(), {});

  return impl;
}

LogicalResult insertCopy(EdgeOp edge) {
  auto type = edge.getIn().getType();
  // type is max of input and output
  if (getTypeSize(edge.getIn()) < getTypeSize(edge.getOut())) {
    type = edge.getOut().getType();
  }

  auto impl = codegenCopyImpl(edge.getOut());

  auto builder = OpBuilder(edge);
  builder.setInsertionPointAfter(edge);

  auto new_edge_1 = CREATE(EdgeOp, builder, edge.getLoc(), type, edge.getIn());
  auto node = CREATE(NodeOp, builder, edge.getLoc(), {type}, impl.getSymName(),
                     {}, new_edge_1.getOut(), {});
  auto new_edge_2 = CREATE(EdgeOp, builder, edge.getLoc(),
                           edge.getOut().getType(), node->getResult(0));
  edge.getOut().replaceAllUsesWith(new_edge_2.getOut());
  new_edge_1["rank"] = *edge["rank"];
  new_edge_1["delay_elems"] = 0;
  new_edge_1["delay_bytes"] = 0;

  new_edge_2["rank"] = (i64)edge["rank"] + 1;
  new_edge_2->setAttr("backwards_edge", UnitAttr::get(edge->getContext()));
  new_edge_2["delay"] = *edge["delay"];
  new_edge_2["delay_elems"] = *edge["delay_elems"];
  new_edge_2["delay_bytes"] = *edge["delay_bytes"];
  edge.erase();
  return success();
}

// Edge is inout if it flows into inout input
LogicalResult LowerToTasksPass::checkNoReverseInoutEdges(ActorOp actor) {
  for (auto edge : actor.getOps<EdgeOp>() | IntoVector()) {
    auto out_use = edge.getOut().getUses().begin();
    if (edge["backwards_edge"] and
        cast<NodeOp>(out_use->getOwner()).isInoutInput(*out_use)) {
      edge->emitError() << "Found a reverse inout edge. These are not "
                           "supported.";
      return failure();
    }
    if (edge.getConsumerNode() == edge.getProducerNode()) {
      insertCopy(edge);
    }
  }
  return success();
}

// Creates the runtime function that takes the iteration number and a buffer
// of pointers to pass to the kernel implementation.
func::FuncOp LowerToTasksPass::createTaskFunc(NodeOp node, StringRef name,
                                              OpBuilder builder) {
  auto kernel_impl_type = node.getKernelFunctionType();

  auto llvm_pointer_type = LLVM::LLVMPointerType::get(builder.getContext());
  // auto num_inputs = kernel_impl_type.getNumInputs();

  Vec<Type> task_input_types = {builder.getI64Type(), builder.getI64Type()};

  assert(node.getIn().empty());
  assert(node.getOut().size() == node.getInout().size());

  for (auto param : node.getParams()) {
    task_input_types.push_back(param.getType());
  }

  for (auto i : node.getInout()) {
    if (auto ranked_tensor = dyn_cast<RankedTensorType>(i)) {
      task_input_types.push_back(MemRefType::get(
          ranked_tensor.getShape(), ranked_tensor.getElementType()));
    } else {
      llvm_unreachable("All ports should be tensors by this point.");
    }
  }

  auto task_func = CREATE(
      func::FuncOp, builder.atBlockBegin(getOperation().getBody()),
      node->getLoc(), name, builder.getFunctionType(task_input_types, {}));

  task_func.setVisibility(mlir::SymbolTable::Visibility::Public);
  task_func->setAttr("llvm.emit_c_interface", builder.getUnitAttr());

  auto task_entry_block = task_func.addEntryBlock();
  auto task_builder = builder.atBlockBegin(task_entry_block);

  SmallVector<Value> kernel_operands;

  auto buffers = task_func.getBody().getArgument(1);

  for (auto [operand_index, kernel_input] :
       llvm::enumerate(kernel_impl_type.getInputs())) {
    if (auto memref = dyn_cast<MemRefType>(kernel_input)) {
      auto offset_pointer =
          CREATE(LLVM::GEPOp, task_builder, node->getLoc(), llvm_pointer_type,
                 llvm_pointer_type, buffers, {int(operand_index)});
      auto operand_val = CREATE(LLVM::LoadOp, task_builder, node.getLoc(),
                                llvm_pointer_type, offset_pointer);

      // kernel_operands.push_back(cast_to_memref.getResult(0));
      kernel_operands.push_back(operand_val.getResult());
    } else {
      // parameter; populate with global
      llvm_unreachable("unimplemented");
    }
  }

  // auto impl_func = CREATE(func::ConstantOp, task_builder,
  // node->getLoc(),
  //                         kernel_impl_type, node.getImpl());

  // Call kernel

  auto kernel_call = CREATE(func::CallOp, task_builder, node.getLoc(),
                            node.getImpl(), {}, kernel_operands);

  getFuncDecl(kernel_call);

  CREATE(func::ReturnOp, task_builder, node.getLoc(), {});

  return task_func;
}

EdgeOp LowerToTasksPass::createEdgeAdaptor(Value produced, Type consumed) {
  auto builder = OpBuilder(produced.getDefiningOp());
  builder.setInsertionPointAfterValue(produced);
  auto edge = CREATE(EdgeOp, builder, produced.getDefiningOp()->getLoc(),
                     consumed, produced);
  return edge;
}

i64 LowerToTasksPass::getIntAttrValue(Operation *op, StringRef name) {
  return op->getAttrOfType<IntegerAttr>(name).getInt();
}

void LowerToTasksPass::populateAllocEdgeAttrs(EdgeOp edge) {
  edge.traitMethod();
  auto next_edge = followInoutEdgeForwards(edge);
  edge["prod_rate"] = -1;
  edge["cons_rate"] = next_edge.getProdRate();
  edge["cons_alpha"] = *next_edge["prod_alpha"];
  edge["cons_beta"] = *next_edge["prod_beta"];
  edge["prod_alpha"] = -1;
  edge["prod_beta"] = -1;
  edge["delay_bytes"] = 0;
  edge["buffer_size_without_delays"] = *next_edge["buffer_size_without_delays"];
  edge["remaining_delay"] = calculateRemainingDelayBytes(edge);
  auto info = getBufferSizeWithDelays(edge);
  edge["buffer_size_with_delays"] = info.total_size;
  edge["total_delay_size"] = info.delays_only;
}

void LowerToTasksPass::populateDeallocEdgeAttrs(EdgeOp edge) {
  auto prev_edge = followInoutEdgeBackwards(edge);
  edge["cons_rate"] = -1;
  edge["prod_rate"] = *prev_edge["cons_rate"];
  edge["cons_alpha"] = -1;
  edge["cons_beta"] = -1;
  edge["prod_alpha"] = *prev_edge["cons_alpha"];
  edge["prod_beta"] = *prev_edge["cons_beta"];
  edge["delay_bytes"] = 0;
  edge["remaining_delay"] = 0;
  edge["buffer_size_with_delays"] = *prev_edge["buffer_size_with_delays"];
  edge["buffer_size_without_delays"] = *prev_edge["buffer_size_without_delays"];
  edge["total_delay_size"] = *prev_edge["total_delay_size"];
}

SmallVector<Value> LowerToTasksPass::createAllocations(ValueRange values) {
  SmallVector<Value> rv;
  for (auto val : values) {
    auto node = val.getDefiningOp<NodeOp>();
    auto builder = OpBuilder(node);
    // Create alloc node
    auto alloc_node = CREATE(
        NodeOp, builder, node->getLoc(),
        {RankedTensorType::get(
            {1}, dyn_cast<RankedTensorType>(val.getType()).getElementType())},
        "iara_runtime_alloc", {}, {}, {});
    auto edge = createEdgeAdaptor(alloc_node.getOut().front(), val.getType());
    rv.push_back(edge.getOut());
  }
  return rv;
}

void LowerToTasksPass::annotateAllocations(SmallVector<Value> &vals) {
  for (auto val : vals) {
    auto edge = cast<EdgeOp>(val.getDefiningOp());
    auto node = cast<NodeOp>(*edge->getUsers().begin());
    auto alloc_node = (NodeOp)edge.getIn().getDefiningOp();
    populateAllocEdgeAttrs(edge);
    // auto first_node_total_firings =
    //     node->getAttrOfType<IntegerAttr>("total_firings").getInt();
    // i64 total_firings =
    //     (first_node_total_firings - getIntAttrValue(edge, "cons_alpha")) /
    //         getIntAttrValue(edge, "cons_beta") +
    //     1;
    // alloc_node["total_firings"] = total_firings;
  }
}

Value getPlaceholderValue(ModuleOp module) {
  return getIntConstant(module.getBody(), 0);
}

SmallVector<NodeOp> LowerToTasksPass::createDeallocations(ValueRange values) {
  SmallVector<NodeOp> rv;
  // Populate with edge id once its generated
  auto placeholder_value = getPlaceholderValue(getOperation());
  for (auto original_val : values) {
    auto edge = createEdgeAdaptor(
        original_val, RankedTensorType::get(
                          {1}, getElementTypeOrSelf(original_val.getType())));
    auto val = edge.getOut();
    auto builder = OpBuilder(edge);
    builder.setInsertionPointAfter(edge);
    // Create dealloc node
    auto dealloc_node =
        CREATE(NodeOp, builder, edge->getLoc(), {}, "iara_runtime_dealloc",
               {placeholder_value}, {val}, {});

    rv.push_back(dealloc_node);
  }
  return rv;
}

void LowerToTasksPass::annotateDeallocations(
    SmallVector<NodeOp> &dealloc_nodes) {
  for (auto dealloc_node : dealloc_nodes) {
    auto edge = cast<EdgeOp>(dealloc_node.getIn().front().getDefiningOp());
    populateDeallocEdgeAttrs(edge);
    auto last_node = edge.getProducerNode();
    // auto last_node_total_firings =
    //     last_node->getAttrOfType<IntegerAttr>("total_firings").getInt();
    // i64 total_firings =
    //     (last_node_total_firings - getIntAttrValue(edge, "prod_alpha")) /
    //         getIntAttrValue(edge, "prod_beta") +
    //     1;
    // dealloc_node["total_firings"] = total_firings;
  }
}

// Replaces nodes that have pure ins or outs with new ones and wire them to
// new alloc and dealloc nodes.
LogicalResult LowerToTasksPass::generateAllocAndFreeNodes(ActorOp actor) {
  for (auto old_node : actor.getOps<NodeOp>() | IntoVector()) {
    if (old_node.getIn().size() == 0 and old_node.getPureOuts().size() == 0)
      continue;
    auto builder = OpBuilder(old_node);

    SmallVector<Value> new_node_inputs = old_node.getAllInputs();
    auto alloc_inputs = createAllocations(old_node.getPureOuts());
    new_node_inputs.append(alloc_inputs);

    SmallVector<Type> new_result_types =
        llvm::to_vector(old_node.getIn().getTypes());
    new_result_types.append(old_node->getResultTypes() | IntoVector());

    auto new_node =
        CREATE(NodeOp, builder, old_node->getLoc(), new_result_types,
               old_node.getImpl(), old_node.getParams(), {}, new_node_inputs);

    assert(new_node_inputs.size() == new_result_types.size());

    new_node->setDiscardableAttrs(old_node->getDiscardableAttrs());

    auto new_outs = new_node.getOut().take_front(old_node.getIn().size());
    auto existing_outs = new_node.getOut().drop_front(old_node.getIn().size());
    auto new_dealloc_nodes = createDeallocations(new_outs);

    for (auto [old, new_] : llvm::zip_equal(old_node.getOut(), existing_outs)) {
      old.replaceAllUsesWith(new_);
    }
    old_node->erase();

    annotateDeallocations(new_dealloc_nodes);
    annotateAllocations(alloc_inputs);
  }
  return success();
}

func::FuncOp LowerToTasksPass::getFuncDecl(func::CallOp call,
                                           bool use_llvm_pointers) {
  auto module = call->getParentOfType<ModuleOp>();
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());

  if (auto func = module.lookupSymbol<func::FuncOp>(call.getCallee()))
    return func;

  auto rv = CREATE(
      func::FuncOp, builder, module.getLoc(), call.getCallee(),
      builder.getFunctionType(call->getOperandTypes(), call->getResultTypes()));
  rv->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
  rv.setVisibility(mlir::SymbolTable::Visibility::Private);
  return rv;
}

void LowerToTasksPass::generateAllocFiring(NodeOp node, OpBuilder builder) {

  auto curr_function = cast<func::FuncOp>(builder.getBlock()->getParentOp());
  auto alloc_call =
      CREATE(func::CallOp, builder, node->getLoc(), "iara_runtime_alloc", {},
             {getIntConstant(&curr_function.getFunctionBody().front(),
                             node->getAttrOfType<IntegerAttr>("node_id"))});
  getFuncDecl(alloc_call);
}

void LowerToTasksPass::codegenInitialFirings(NodeOp node, OpBuilder builder) {
  assert(node.getImpl() == "iara_runtime_alloc");

  auto alloc_edge = followChainUntilNext<EdgeOp>(node.getResult(0));

  generateAllocFiring(node, builder);
}

i64 LowerToTasksPass::getTotalInputBytes(NodeOp node) {
  i64 rv = 0;
  for (auto input : node.getAllInputs()) {
    rv += getTypeSize(input);
  }
  return rv;
}

std::string LowerToTasksPass::getTaskFuncName(NodeOp node,
                                              OpBuilder func_builder) {
  if (node.isAlloc() or node.isDealloc()) {
    return {}; // No need to generate a task for dealloc nodes.
  }
  auto str = std::string{llvm::formatv(
      "iara_task_{0}_{1}", node->getAttrOfType<IntegerAttr>("node_id").getInt(),
      node.getImpl())};
  createTaskFunc(node, str, func_builder);
  return str;
};

LogicalResult LowerToTasksPass::codegenRuntimeInit(OpBuilder builder) {
  auto call = CREATE(func::CallOp, builder, builder.getUnknownLoc(),
                     "iara_runtime_init", {}, {});
  getFuncDecl(call);
  return success();
}

LogicalResult LowerToTasksPass::codegenRuntimeFunction(ActorOp actor) {
  // todo: generate runtime header
  auto module = getOperation();
  auto builder = OpBuilder(module).atBlockBegin(module.getBody());
  auto func_builder = OpBuilder{builder};
  auto setup_function =
      CREATE(func::FuncOp, builder, actor->getLoc(), actor.getSymName(),
             builder.getFunctionType(
                 {builder.getI64Type(), builder.getI64Type()}, {}));

  setup_function->setAttr("sym_visibility", builder.getStringAttr("public"));
  setup_function->setAttr("llvm.emit_c_interface", builder.getUnitAttr());

  auto entry_block = setup_function.addEntryBlock();
  builder = builder.atBlockBegin(entry_block);

  DenseMap<NodeOp, i64> totalInputBytes;

  for (NodeOp node : actor.getOps<NodeOp>()) {
    totalInputBytes[node] = 0;
    for (auto i : node.getAllInputs()) {
      totalInputBytes[node] += mlir_util::getTypeTokenCount(i.getType());
    }
  }

  // auto create_runtime_call = CREATE(func::CallOp, builder, actor->getLoc(),
  //                                   "iara_runtime_createRuntime", {}, {});

  // getFuncDecl(create_runtime_call);

  auto all_nodes = actor.getOps<NodeOp>() | IntoVector();
  auto all_edges = actor.getOps<EdgeOp>() | IntoVector();

  codegenHeaderFile(func_builder, "edge_data.inc.h", all_nodes, all_edges);

  // generate first calls (assume all sources are alloc nodes)

  codegenRuntimeInit(builder);

  for (auto node : actor.getOps<NodeOp>()) {
    if (node->getNumOperands() > 0)
      continue;

    if (node.getImpl() != "iara_runtime_alloc") {
      node->emitError(
          "Trivial case of no edges is unimplemented at the moment.");
      return failure();
    }

    codegenInitialFirings(node, builder);
  }

  auto wait_call = CREATE(func::CallOp, builder, actor->getLoc(),
                          "iara_wait_graph_iteration", {}, {});

  getFuncDecl(wait_call);

  CREATE(func::ReturnOp, builder, actor->getLoc(), {});

  return success();
}

LogicalResult LowerToTasksPass::taskify(ActorOp actor) {
  if (not actor.isFlat())
    return failure();
  if (actor.isKernelDeclaration())
    return failure();
  if (actor.isEmpty())
    return failure();
  if (expandImplicitEdges(actor).failed())
    return failure();
  if (mlir::iara::canon::canonicalizeTypes(actor).failed())
    return failure();
  if (annotateEdgeInfo(actor).failed())
    return failure();
  if (checkNoReverseInoutEdges(actor).failed())
    return failure();
  if (allocateContiguousBuffers(actor).failed())
    return failure();
  if (generateAllocAndFreeNodes(actor).failed())
    return failure();
  if (annotateIDs(actor).failed())
    return failure();
  // if (annotateTotalFirings(actor).failed())
  //   return failure();
  if (calculateTotalFirings(actor).failed())
    return failure();
  if (codegenRuntimeFunction(actor).failed())
    return failure();
  actor.erase();
  return success();
}

void LowerToTasksPass::runOnOperation() {
  auto module = getOperation();

  module->walk([&](ActorOp actor_op) {
    if (not actor_op.isFlat() and not actor_op.isKernelDeclaration()) {
      module.emitError("Actor ")
          << actor_op.getSymName() << " is not flattened.";
      signalPassFailure();
      return;
    }
  });
  module->walk([&](ActorOp actor_op) {
    if (taskify(actor_op).failed()) {
      module.emitError("Failed to lower all actors to task form.");
      signalPassFailure();
      return;
    };
  });
}

} // namespace mlir::iara::passes
