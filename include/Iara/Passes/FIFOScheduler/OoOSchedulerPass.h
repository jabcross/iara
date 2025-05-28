#ifndef IARA_PASSES_LOWERTOFIFO_H
#define IARA_PASSES_LOWERTOFIFO_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
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

struct OoOSchedulerPass
    : public PassWrapper<OoOSchedulerPass, OperationPass<::mlir::ModuleOp>> {
  struct Impl;
  ::llvm::StringRef getArgument() const override { return "ooo-scheduler"; }
  ::llvm::StringRef getDescription() const override {
    return "Converts SDF dataflow to a runtime with out-of-order FIFOs.";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("OoOSchedulerPass");
  }

  Impl *pimpl;

  ::llvm::StringRef getName() const override { return "OoOSchedulerPass"; }

  void runOnOperation() final override;
};

inline void registerOoOSchedulerPass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<iara::passes::fifo::OoOSchedulerPass>();
  });
}

} // namespace iara::passes::fifo

#endif
