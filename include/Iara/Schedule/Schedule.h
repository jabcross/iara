#ifndef DATAFLOW_PASSES_H
#define DATAFLOW_PASSES_H

#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/BuiltinDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir::iara {

class SchedulePass
    : public mlir::PassWrapper<SchedulePass,
                               mlir::OperationPass<mlir::ModuleOp>> {

public:
  // Option<BufferizationStrategy> bufferization{
  //     *this, "bufferization_strategy",
  //     llvm::cl::desc("Bufferization strategy."
  //                    "POOL uses a memory pool to reuse memory after tokens
  //                    are " "consumed. OMP_MALLOC uses malloc and OpenMP
  //                    tasks.")};

  // SDFSchedulePass(
  //     BufferizationStrategy buf_strat = BufferizationStrategy::POOL) {
  //   this->bufferization = buf_strat;
  // };

  // SDFSchedulePass(SDFSchedulePass const &cloned) {
  //   this->bufferization = cloned.bufferization;
  // }
  virtual ~SchedulePass() = default;

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect, memref::MemRefDialect, mlir::func::FuncDialect,
                    arith::ArithDialect, func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect>();
  }
  void runOnOperation() final;
};

class Scheduler {
public:
  virtual ~Scheduler() = default;
  virtual LogicalResult emit(ModuleOp module) = 0;
  virtual LogicalResult schedule() = 0;
};

} // namespace mlir::iara

#endif