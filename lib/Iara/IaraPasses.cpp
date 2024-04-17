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
#include "Iara/Schedule/OpenMPScheduler.h"
#include "Iara/Schedule/TaskScheduler.h"
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
    for (auto actor : module.getOps<ActorOp>()) {
      if (!actor.getOps<NodeOp>().empty()) {
        if (!actor->hasAttr("flat")) {
          actor->emitRemark(
              "Actor has not been flattened. Run --iara-flatten first.");
          signalPassFailure();
          continue;
        }

        TaskScheduler task_scheduler{};

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
