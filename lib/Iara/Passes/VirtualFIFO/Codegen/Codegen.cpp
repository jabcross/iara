#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/Common/Codegen/AsValue.h"
#include "Iara/Passes/Common/Codegen/Codegen.h"
#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Internal.h"
#include "Iara/Passes/VirtualFIFO/Codegen/StaticDataEmitStrategy.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include <cassert>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FormatVariadic.h>
#include <memory>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/MLIRContext.h>
#include <span>

namespace iara::passes::virtualfifo::codegen {
using namespace iara::passes::virtualfifo::sdf;
using namespace iara::util::mlir;
using namespace mlir::LLVM;
using namespace iara::dialect;
using namespace iara::passes::common::codegen;

void fillOutPairPointers(std::span<NodeCodegenData> node_codegen_datas,
                         std::span<EdgeCodegenData> edge_codegen_datas) {
  DenseMap<NodeOp, NodeCodegenData *> node_to_data;
  DenseMap<EdgeOp, EdgeCodegenData *> edge_to_data;

  for (auto &node_codegen_data : node_codegen_datas) {
    node_to_data[node_codegen_data.node_op] = &node_codegen_data;
  }
  for (auto &edge_codegen_data : edge_codegen_datas) {
    edge_to_data[edge_codegen_data.edge_op] = &edge_codegen_data;
  }

  for (auto &node_codegen_data : node_codegen_datas) {
    for (auto input : node_codegen_data.node_op.getAllInputs()) {
      auto edge_op = llvm::cast<EdgeOp>(input.getDefiningOp());
      assert(edge_to_data.contains(edge_op));
      auto edge_codegen_data = edge_to_data[edge_op];
      node_codegen_data.inputs.push_back(edge_codegen_data);
      edge_codegen_data->consumer = &node_codegen_data;
    }
    for (auto output : node_codegen_data.node_op.getAllOutputs()) {
      auto users = llvm::to_vector(output.getUsers());
      assert(users.size() == 1);
      auto edge_op = llvm::cast<EdgeOp>(users.front());
      assert(edge_to_data.contains(edge_op));
      auto edge_codegen_data = edge_to_data[edge_op];
      node_codegen_data.outputs.push_back(edge_codegen_data);
      edge_codegen_data->producer = &node_codegen_data;
    }
  }
  for (auto &edge_codegen_data : edge_codegen_datas) {
    edge_codegen_data.alloc_node =
        node_to_data[findFirstNodeOfChain(edge_codegen_data.edge_op)];
    assert(edge_codegen_data.alloc_node->node_op.isAlloc());

    auto next_edge_op = followInoutChainForwards(edge_codegen_data.edge_op);
    if (next_edge_op) {
      auto data = edge_to_data[next_edge_op];
      edge_codegen_data.next_edge = data;
    }
  }
}

std::string getDebugName(NodeOp node) {
  if (node.isAlloc()) {
    auto alloc_edge = cast<EdgeOp>(*node->getResult(0).getUsers().begin());
    auto first_node = getConsumerNode(alloc_edge);
    auto index = alloc_edge->getUses().begin()->getOperandNumber();
    return llvm::formatv("allocNode_{3}_{2}_{0}[{1}]\0",
                         first_node.getImpl(),
                         index,
                         (void*)first_node.getOperation(),
                         (void*)node.getOperation());
  }
  if (node.isDealloc()) {
    auto dealloc_edge = cast<EdgeOp>(node->getOperand(0).getDefiningOp());
    auto last_node = getProducerNode(dealloc_edge);
    auto index = util::mlir::getResultIndex(dealloc_edge.getIn());
    return llvm::formatv("deallocNode_{3}_{2}_{0}[{1}]\0",
                         last_node.getImpl(),
                         index,
                         (void*)last_node.getOperation(),
                         (void*)node.getOperation());
  }
  return llvm::formatv("node_{0}_{1}\0", (void*)node.getOperation(), node.getImpl());
}

std::string getDebugName(EdgeOp edge) {
  auto prod = getProducerNode(edge);
  auto cons = getConsumerNode(edge);
  auto prod_index = util::mlir::getResultIndex(edge.getIn());
  auto cons_index = edge->getUses().begin()->getOperandNumber();
  if (prod.isAlloc()) {
    return llvm::formatv("allocEdge_{3}_{0}_{1}[{2}]\0",
                         cons.getImpl(),
                         (void*)edge.getOperation(),
                         cons_index,
                         (void*)prod.getOperation());
  }
  if (cons.isDealloc()) {
    return llvm::formatv("deallocNode_{3}_{0}_{1}[{2}]\0",
                         prod.getImpl(),
                         (void*)edge.getOperation(),
                         prod_index,
                         (void*)cons.getOperation());
  }
  return llvm::formatv("edge_{6}_{4}_{0}[{1}]->{5}_{2}[{3}]\0",
                       prod.getImpl(),
                       prod_index,
                       cons.getImpl(),
                       cons_index,
                       (void*)prod.getOperation(),
                       (void*)cons.getOperation(),
                       (void*)edge.getOperation());
}

struct CodegenStaticData::Impl {
  ModuleOp module;
  OpBuilder module_builder;
  std::span<NodeCodegenData> node_pairs;
  std::span<EdgeCodegenData> edge_codegen_datas;
  std::unique_ptr<StaticDataEmitStrategy> strategy;

  Impl(ModuleOp module,
       OpBuilder module_builder,
       std::span<NodeCodegenData> node_pairs,
       std::span<EdgeCodegenData> edge_pairs)
      : module(module), module_builder(module_builder), node_pairs(node_pairs),
        edge_codegen_datas(edge_pairs), strategy(makeEmbedSidecarStrategy()) {
    fillOutPairPointers(node_pairs, edge_pairs);
  }

  void codegenStaticData() {
    strategy->emit(module, module_builder, node_pairs, edge_codegen_datas);
  }
};

CodegenStaticData::CodegenStaticData(ModuleOp module,
                                     OpBuilder module_builder,
                                     std::span<NodeCodegenData> node_pairs,
                                     std::span<EdgeCodegenData> edge_pairs) {
  pimpl = new Impl(module, module_builder, node_pairs, edge_pairs);
}

void CodegenStaticData::codegenStaticData() { pimpl->codegenStaticData(); }

CodegenStaticData::~CodegenStaticData() { delete pimpl; }

CodegenStaticData::CodegenStaticData(CodegenStaticData &&data) {
  pimpl = data.pimpl;
  data.pimpl = nullptr;
}

} // namespace iara::passes::virtualfifo::codegen
