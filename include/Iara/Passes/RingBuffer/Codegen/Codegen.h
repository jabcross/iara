#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Iara/Dialect/IaraOps.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include "mlir/IR/Builders.h"
#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>
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

namespace iara::passes::ringbuffer::codegen {
using namespace util::mlir;
using namespace LLVM;
using namespace iara::dialect;

struct CodegenStaticData {

  struct Impl;

  Impl *pimpl;

  CodegenStaticData(ModuleOp module,
                    OpBuilder module_builder,
                    std::span<RingBuffer_Node> node_infos,
                    std::span<RingBuffer_Edge> edge_infos,
                    std::span<NodeOp> nodes,
                    std::span<EdgeOp> edges,
                    std::span<LLVMFuncOp> wrappers);

  // std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  void codegenStaticData();

  CodegenStaticData(CodegenStaticData &data) = delete;
  CodegenStaticData(CodegenStaticData &&data);
  ~CodegenStaticData();
};

Value createConstOp(OpBuilder builder, Location loc, auto val);
Value getEmptySpan(OpBuilder builder, Location loc);

} // namespace iara::passes::ringbuffer::codegen
#endif
