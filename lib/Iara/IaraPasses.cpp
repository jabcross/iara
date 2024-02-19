//===- IaraPasses.cpp - Iara passes -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "Iara/IaraPasses.h"

namespace mlir::iara {
#define GEN_PASS_DEF_IARASWITCHBARFOO
#include "Iara/IaraPasses.h.inc"

namespace {
class IaraSwitchBarFooRewriter : public OpRewritePattern<func::FuncOp> {
public:
  using OpRewritePattern<func::FuncOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(func::FuncOp op,
                                PatternRewriter &rewriter) const final {
    if (op.getSymName() == "bar") {
      rewriter.updateRootInPlace(op, [&op]() { op.setSymName("foo"); });
      return success();
    }
    return failure();
  }
};

class IaraSwitchBarFoo : public impl::IaraSwitchBarFooBase<IaraSwitchBarFoo> {
public:
  using impl::IaraSwitchBarFooBase<IaraSwitchBarFoo>::IaraSwitchBarFooBase;
  void runOnOperation() final {
    RewritePatternSet patterns(&getContext());
    patterns.add<IaraSwitchBarFooRewriter>(&getContext());
    FrozenRewritePatternSet patternSet(std::move(patterns));
    if (failed(applyPatternsAndFoldGreedily(getOperation(), patternSet)))
      signalPassFailure();
  }
};
} // namespace
} // namespace mlir::iara
