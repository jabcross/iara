#ifndef IARA_PASSES_LOWERTOFIFO_H
#define IARA_PASSES_LOWERTOFIFO_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include <mlir/Analysis/Presburger/Matrix.h>
#include <mlir/Dialect/DLTI/DLTI.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/OpenMP/OpenMPDialect.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/Builders.h>
#include <mlir/Pass/Pass.h>

namespace iara::passes::fifo {

using namespace iara::util::range;
using namespace iara::util::mlir;
using util::Rational;
template <class T> using Vec = llvm::SmallVector<T>;
template <class T> using Pair = std::pair<T, T>;
using mlir::presburger::IntMatrix;
using mlir::presburger::MPInt;

struct DynamicPushFirstFIFOSchedulerPass
    : public PassWrapper<DynamicPushFirstFIFOSchedulerPass,
                         OperationPass<::mlir::ModuleOp>> {
  struct Impl;
  ::llvm::StringRef getArgument() const override {
    return "dynamic-push-first-fifo-scheduler";
  }
  ::llvm::StringRef getDescription() const override {
    return "Converts SDF dataflow to a runtime with dynamic FIFOs. This "
           "version has non-blocking pushs and blocking pops.";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("DynamicPushFirstFIFOSchedulerPass");
  }

  Impl *pimpl;

  ::llvm::StringRef getName() const override {
    return "DynamicFIFOSchedulerPass";
  }

  void runOnOperation() final override;
};

inline void registerDynamicPushFirstFIFOSchedulerPass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<
        iara::passes::fifo::DynamicPushFirstFIFOSchedulerPass>();
  });
}

} // namespace iara::passes::fifo

#endif
