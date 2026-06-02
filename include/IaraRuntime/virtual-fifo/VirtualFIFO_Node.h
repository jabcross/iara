#ifndef IARA_RUNTIME_SDF_NODE_H
#define IARA_RUNTIME_SDF_NODE_H

// Strategy B (embed sidecar) is the canonical layout.
// The former inline-strategy layout is no longer supported.
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node_Embed.h"

#ifdef IARA_COMPILER
  #include "Iara/Passes/Common/Codegen/GetMLIRType.h"
namespace iara::passes::common::codegen {
template <> struct GetMLIRType<VirtualFIFO_Node_Semaphore> {
  static mlir::Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};
} // namespace iara::passes::common::codegen
#endif

#endif // IARA_RUNTIME_SDF_NODE_H
