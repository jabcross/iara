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
#include "Iara/Passes/Schedule/OpenMPScheduler.h"
#include "Iara/Passes/Schedule/TaskScheduler.h"
#include "llvm/Support/Casting.h"
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>

namespace mlir::iara {
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
    module->dump();
    for (auto actor : module.getOps<ActorOp>()) {

      // Only schedule top-level actors.
      if (!actor.getOps<NodeOp>().empty() and
          actor.getOps<InPortOp>().empty() and
          actor.getOps<OutPortOp>().empty()) {

        if (!actor->hasAttr("flat")) {
          actor->emitError(
              "Actor has not been flattened. Run --iara-flatten first.");
          signalPassFailure();
          return;
        }

        TaskScheduler task_scheduler{};

        if (!task_scheduler.checkSingleRate(actor)) {
          actor->emitError("Actor is not single rate (Not supported yet).");
          signalPassFailure();
          return;
        }

        if (task_scheduler.convertToTasks(actor).failed()) {
          module.emitError("Failed to convert actor into task form");
          signalPassFailure();
          return;
        }
      }

      OpenMPScheduler omp_scheduler{};

      omp_scheduler.convertIntoOpenMP(actor);
    }

    if (TaskScheduler{}.removeLoweredActors(module).failed()) {
      module.emitError("Failed to remove all actors");
      signalPassFailure();
      return;
    }
  }
};

} // namespace
} // namespace mlir::iara
