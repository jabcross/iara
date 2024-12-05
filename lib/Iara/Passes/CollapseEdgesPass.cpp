//===- IaraPasses.cpp - Iara passes -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "Iara/IaraPasses.h"
#include "Iara/Passes/Schedule/OpenMPScheduler.h"
#include "Iara/Passes/Schedule/TaskScheduler.h"
#include "Util/RangeUtil.h"
#include "llvm/Support/Casting.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/Location.h>
#include <mlir/Support/LLVM.h>

using namespace RangeUtil;

namespace mlir::iara::passes {
#define GEN_PASS_DEF_COLLAPSEEDGESPASS
#include "Iara/IaraPasses.h.inc"

namespace {

class CollapseEdgesPass
    : public impl::CollapseEdgesPassBase<CollapseEdgesPass> {
public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect, memref::MemRefDialect, mlir::func::FuncDialect,
                    arith::ArithDialect, func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect>();
  }
  using impl::CollapseEdgesPassBase<CollapseEdgesPass>::CollapseEdgesPassBase;

  void runOnOperation() final {
    auto module = getOperation();
    SmallVector<EdgeOp> to_erase;
    module.walk([&](EdgeOp edge) {
      if (edge.getOut().getType() == edge.getIn().getType() and
          !edge->hasAttr("delay")) {
        edge.getOut().replaceAllUsesWith(edge.getIn());
        to_erase.push_back(edge);
      }
    });
    for (auto edge : to_erase) {
      edge.erase();
    }
  }
};

} // namespace
} // namespace mlir::iara::passes
