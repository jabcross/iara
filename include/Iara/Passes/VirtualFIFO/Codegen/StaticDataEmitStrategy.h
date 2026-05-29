#ifndef IARA_PASSES_VIRTUALFIFO_CODEGEN_STATIC_DATA_EMIT_STRATEGY_H
#define IARA_PASSES_VIRTUALFIFO_CODEGEN_STATIC_DATA_EMIT_STRATEGY_H

#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include <memory>
#include <span>

namespace iara::passes::virtualfifo::codegen {

struct StaticDataEmitStrategy {
  virtual ~StaticDataEmitStrategy() = default;

  virtual void emit(mlir::ModuleOp module,
                    mlir::OpBuilder module_builder,
                    std::span<NodeCodegenData> nodes,
                    std::span<EdgeCodegenData> edges) = 0;
};

std::unique_ptr<StaticDataEmitStrategy> makeMlirInlineStrategy();

} // namespace iara::passes::virtualfifo::codegen

#endif
