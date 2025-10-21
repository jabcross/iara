#include "Iara/Passes/VirtualFIFO/SDF/VirtualFIFOAnalysis.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/VirtualFIFO/SDF/BufferSizeCalculator.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include <cassert>
#include <format>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <print>

namespace iara::passes::virtualfifo::sdf {

std::string getDelayCopyName() { return "iara_delay_copy"; }

LLVM::LLVMFuncOp getOrCodegenDelayCopyImpl(ModuleOp module, Location loc) {
  auto name = getDelayCopyName();

  if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(name)) {
    return existing;
  }

  auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

  auto i64_type = mod_builder.getI64Type();
  auto llvm_ptr = LLVM::LLVMPointerType::get(module->getContext());

  Vec<Type> arg_types = {llvm_ptr /*state*/,
                         i64_type /*input size*/,
                         i64_type /*output size*/,
                         i64_type /*delay size*/,
                         llvm_ptr /*input ptr*/,
                         llvm_ptr /*output ptr*/};

  DEF_OP(LLVM::LLVMFuncOp,
         impl,
         LLVM::LLVMFuncOp,
         mod_builder,
         loc,
         name,
         LLVM::LLVMFunctionType::get(
             LLVM::LLVMVoidType::get(module->getContext()), arg_types));
  impl.setVisibility(mlir::SymbolTable::Visibility::Public);
  impl->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
  return impl;
}

// Converts a delay into a node and returns the two new edges.
PairOf<EdgeOp> insertDelayCopy(Vec<EdgeOp> &chain, StaticAnalysisData &data) {
  Vec<EdgeOp> edges_with_buffers;
  for (auto edge : chain) {
    if (data.edge_static_info[edge].delay_size > 0) {
      edges_with_buffers.push_back(edge);
    }
  }

  // Let's just worry about the simple case for now.
  assert(edges_with_buffers.size() == 1);

  auto edge = edges_with_buffers[0];

  auto builder = OpBuilder(edge);

  DEF_OP(EdgeOp,
         new_edge_in,
         EdgeOp,
         builder,
         edge->getLoc(),
         edge.getIn().getType(),
         edge.getIn());

  auto input_size = util::mlir::getTypeSize(edge.getIn());
  auto output_size = util::mlir::getTypeSize(edge.getOut());
  auto delay_size = getDelaySizeBytes(edge);

  auto input_size_const =
      util::mlir::getIntConstant(edge->getBlock(), input_size);
  auto output_size_const =
      util::mlir::getIntConstant(edge->getBlock(), output_size);
  auto delay_size_const =
      util::mlir::getIntConstant(edge->getBlock(), delay_size);

  DEF_OP(Value,
         placeholder_ptr,
         LLVM::ZeroOp,
         builder,
         edge->getLoc(),
         LLVM::LLVMPointerType::get(edge->getContext()));

  DEF_OP(
      NodeOp,
      delay_copy,
      NodeOp,
      builder,
      edge->getLoc(),
      {edge.getOut().getType()},
      getDelayCopyName(),
      {placeholder_ptr, input_size_const, output_size_const, delay_size_const},
      {new_edge_in.getOut()},
      {});

  DEF_OP(EdgeOp,
         new_edge_out,
         EdgeOp,
         builder,
         edge->getLoc(),
         edge.getOut().getType(),
         delay_copy->getResult(0));

  edge.getOut().replaceAllUsesWith(new_edge_out.getOut());

  data.edge_static_info.erase(edge);

  LLVM::LLVMFuncOp _ = getOrCodegenDelayCopyImpl(
      edge->getParentOfType<ModuleOp>(), edge.getLoc());

  edge.erase();
  return {new_edge_in, new_edge_out};
}

LogicalResult solveProblematicChain(Vec<EdgeOp> &chain,
                                    StaticAnalysisData &data) {
  auto [in_edge, out_edge] = insertDelayCopy(chain, data);
  auto in_chain = getInoutChain(in_edge);
  auto out_chain = getInoutChain(out_edge);
  return success(analyzeVirtualInoutChain(in_chain, data).succeeded() &&
                 analyzeVirtualInoutChain(out_chain, data).succeeded());
}

LogicalResult analyzeVirtualInoutChain(Vec<EdgeOp> &chain,
                                       StaticAnalysisData &data) {

  assert(chain.size() > 0);

  Vec<i64> rates, delays;

  rates.push_back(getProdRateBytes(chain.front()));

  for (auto edge : chain) {
    auto &info = data.edge_static_info[edge];
    info.cons_rate = getConsRateBytes(edge);
    info.prod_rate = getProdRateBytes(edge);
    info.delay_size = getDelaySizeBytes(edge);
    rates.push_back(info.cons_rate);
    delays.push_back(info.delay_size);
  }

  assert(rates.size() == chain.size() + 1);
  assert(delays.size() == chain.size());

  auto buffer_values = calculateBufferSize(data.memo, rates, delays);
  if (failed(buffer_values)) {
    // Problematic case.
    return solveProblematicChain(chain, data);
  }

  auto first_buffer_size = buffer_values.value()->alpha.back() * rates.back();
  auto next_buffer_sizes = buffer_values.value()->beta.back() * rates.back();

  i64 offset = 0;
  for (i64 i = chain.size() - 1; i >= 0; i--) {
    auto edge = chain[i];
    auto &info = data.edge_static_info[edge];
    info.delay_size = getDelaySizeBytes(edge);
    info.delay_offset = offset;
    offset += info.delay_size;
    info.block_size_with_delays = first_buffer_size;
    info.block_size_no_delays = next_buffer_sizes;
    info.prod_alpha = buffer_values.value()->alpha[i];
    info.prod_beta = buffer_values.value()->beta[i];
    info.cons_alpha = buffer_values.value()->alpha[i + 1];
    info.cons_beta = buffer_values.value()->beta[i + 1];
  }

  return success();
}

} // namespace iara::passes::virtualfifo::sdf