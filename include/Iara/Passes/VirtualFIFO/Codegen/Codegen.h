#ifndef IARA_PASSES_VIRTUALFIFO_CODEGEN_H
#define IARA_PASSES_VIRTUALFIFO_CODEGEN_H

#include "Iara/Dialect/IaraOps.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include "mlir/IR/Builders.h"
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Dominance.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>

namespace iara::passes::virtualfifo::codegen {
using namespace util::mlir;
using namespace LLVM;
using namespace iara::dialect;

std::string getDebugName(NodeOp edge);
std::string getDebugName(EdgeOp edge);

struct NodeCodegenData;
struct EdgeCodegenData;

struct NodeCodegenData {
  i64 index = -1;
  NodeOp node_op = nullptr;
  VirtualFIFO_Node::StaticInfo static_info;
  LLVMFuncOp wrapper = nullptr;
  std::string name;
  std::vector<EdgeCodegenData *> inputs = {};
  std::vector<EdgeCodegenData *> outputs = {};
  Value input_fifos_span_ptr = {};
  Value output_fifos_span_ptr = {};
};

struct EdgeCodegenData {
  i64 index = -1;
  EdgeOp edge_op = nullptr;
  VirtualFIFO_Edge::StaticInfo static_info;
  NodeCodegenData *consumer = nullptr;
  NodeCodegenData *producer = nullptr;
  NodeCodegenData *alloc_node = nullptr;
  EdgeCodegenData *next_edge = nullptr;
  Value producer_node_ptr = {};
  Value consumer_node_ptr = {};
  Value alloc_node_ptr = {};
  Value next_edge_ptr = {};
};

struct CodegenStaticData {

  struct Impl;

  Impl *pimpl;

  CodegenStaticData(ModuleOp module,
                    OpBuilder module_builder,
                    std::span<NodeCodegenData> node_pairs,
                    std::span<EdgeCodegenData> edge_pairs);

  // std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  void codegenStaticData();

  CodegenStaticData(CodegenStaticData &data) = delete;
  CodegenStaticData(CodegenStaticData &&data);
  ~CodegenStaticData();
};

Value createConstOp(OpBuilder builder, Location loc, auto val);
Value getEmptySpan(OpBuilder builder, Location loc);

} // namespace iara::passes::virtualfifo::codegen
#endif
