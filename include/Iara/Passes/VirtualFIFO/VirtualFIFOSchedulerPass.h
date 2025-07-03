#ifndef IARA_PASSES_VIRTUALFIFO_VIRTUALFIFOSCHEDULERPASS_H
#define IARA_PASSES_VIRTUALFIFO_VIRTUALFIFOSCHEDULERPASS_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Scheduler.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Argument.h>
#include <llvm/Support/CommandLine.h>
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
#include <mlir/Support/LLVM.h>

namespace iara::passes::virtualfifo {

using namespace iara::util::range;
using namespace iara::util::mlir;
using mlir::presburger::IntMatrix;
using util::Rational;

struct VirtualFIFOSchedulerPass
    : public PassWrapper<VirtualFIFOSchedulerPass,
                         OperationPass<::mlir::ModuleOp>> {

  VirtualFIFOSchedulerPass() = default;
  VirtualFIFOSchedulerPass(const VirtualFIFOSchedulerPass &pass) {};

  Option<std::string> main_actor{
      *this, "main-actor", llvm::cl::desc("Name of actor to schedule")};

  struct Impl;
  ::llvm::StringRef getArgument() const override { return "virtual-fifo"; }
  ::llvm::StringRef getDescription() const override {
    return "Converts SDF dataflow to a runtime with virtual FIFOs.";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("VirtualFIFOSchedulerPass");
  }

  Impl *pimpl;

  ::llvm::StringRef getName() const override {
    return "VirtualFIFOSchedulerPass";
  }

  void runOnOperation() final override;
};

inline void registerVirtualFIFOSchedulerPass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<
        iara::passes::virtualfifo::VirtualFIFOSchedulerPass>();
  });
}

} // namespace iara::passes::virtualfifo

#endif
