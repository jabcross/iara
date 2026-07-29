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
  LLVMFuncOp wrapper = nullptr;
  u8 kernel_id = 0; // index into the per-module iara_dispatch_kernel switch
  std::string name;
  // Data (kernel-arg) input edges only. num_args == inputs.size(); the inout
  // output chain and kernel arg-fill both index this list.
  std::vector<EdgeCodegenData *> inputs = {};
  std::vector<EdgeCodegenData *> outputs = {};
  // Logic (control-only) output edges. Not part of the inout-chain output
  // enumeration; the producer pushes a token on each in fire().
  std::vector<EdgeCodegenData *> logic_outputs = {};
  // Logic (control-only) INPUT edges (a join's reader tokens). Not kernel args,
  // so excluded from `inputs`/num_args, but they gate firing: the embed appends
  // them to this node's input-fifo slice (after the data inputs) so the runtime
  // firing-threshold loop sums their cons_rate directly, reconstructing the old
  // `logic_in_bytes` field for free (they carry cons_arg_idx == -1). See
  // EmbedSidecarStrategy and VirtualFIFO_Node::inputDependencyBytes.
  std::vector<EdgeCodegenData *> logic_inputs = {};
  Value input_fifos_span_ptr = {};
  Value output_fifos_span_ptr = {};
};

struct EdgeCodegenData {
  i64 index = -1;
  EdgeOp edge_op = nullptr;
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
