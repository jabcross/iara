//===- IaraPasses.cpp - Iara passes -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "Iara/IaraPasses.h"
#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "llvm/Support/Casting.h"
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>

namespace mlir::iara {
#define GEN_PASS_DEF_IARASWITCHBARFOO
#define GEN_PASS_DEF_IARASCHEDULE
#include "Iara/IaraPasses.h.inc"

namespace {

class IaraSchedule : public impl::IaraScheduleBase<iara::IaraSchedule> {
public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<iara::IaraDialect, memref::MemRefDialect,
                    mlir::func::FuncDialect, arith::ArithDialect,
                    func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect>();
  }

  using impl::IaraScheduleBase<IaraSchedule>::IaraScheduleBase;
  void runOnOperation() final {
    auto module = getOperation();
    auto graph =
        llvm::dyn_cast_if_present<ActorOp>(module.lookupSymbol("main"));

    if (!graph) {
      module.emitError("main graph not found");
      signalPassFailure();
      return;
    }

    if (!graph->hasAttr("flat")) {
      module.emitError(
          "Graph has not been flattened. Run --iara-flatten first.");
      signalPassFailure();
      return;
    }

    // Delete all graphs other than Main.

    llvm::SmallVector<Operation *> to_delete;
    for (Operation &op : *module.getBody()) {
      if (llvm::isa<ActorOp>(op) && &op != graph && !op.hasAttr("kernel"))
        to_delete.push_back(&op);
    }

    for (auto op : to_delete) {
      op->erase();
    }

    auto task_scheduler = TaskScheduler(graph);

    task_scheduler.convertToTasks();

    mlir::bufferization::runOneShotBufferize(module.lookupSymbol("main"), {});

    if (task_scheduler.convertToTasks().failed()) {
      module.emitError("Failed to convert main actor into task form");
      signalPassFailure();
      return;
    }
    if (task_scheduler.convertIntoSequential().failed()) {
      module.emitError("Failed to emit OpenMP scheduler");
      signalPassFailure();
      return;
    }
  }
};

} // namespace
} // namespace mlir::iara
