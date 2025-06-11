#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "mlir/IR/Builders.h" #include < bits / types / locale_t.h>
#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/FormatVariadic.h>
#include <memory>
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
#include <type_traits>

namespace iara::codegen {
using namespace util::mlir;
using namespace LLVM;
using namespace iara::dialect;

struct CodegenStaticData {

  struct Impl;

  Impl *pimpl;

  CodegenStaticData(ModuleOp module,
                    OpBuilder module_builder,
                    std::span<SDF_OoO_Node> node_infos,
                    std::span<SDF_OoO_FIFO> edge_infos,
                    std::span<NodeOp> nodes,
                    std::span<EdgeOp> edges,
                    std::span<LLVMFuncOp> wrappers);

  std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  codegenStaticData();

  CodegenStaticData(CodegenStaticData &data) = delete;
  CodegenStaticData(CodegenStaticData &&data);
  ~CodegenStaticData();
};

} // namespace iara::codegen
#endif
