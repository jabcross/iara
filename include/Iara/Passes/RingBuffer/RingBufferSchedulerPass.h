#ifndef IARA_PASSES_RINGBUFFER_RINGBUFFERSCHEDULERPASS_H
#define IARA_PASSES_RINGBUFFER_RINGBUFFERSCHEDULERPASS_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
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

namespace iara::passes::ringbuffer {

using namespace iara::util::range;
using namespace iara::util::mlir;
using mlir::presburger::IntMatrix;
using mlir::presburger::MPInt;
using util::Rational;

struct RingBufferSchedulerPass
    : public PassWrapper<RingBufferSchedulerPass,
                         OperationPass<::mlir::ModuleOp>> {

  RingBufferSchedulerPass() = default;
  RingBufferSchedulerPass(const RingBufferSchedulerPass &pass) {};

  Option<std::string> main_actor{
      *this, "main-actor", llvm::cl::desc("Name of actor to schedule")};

  struct Impl;
  ::llvm::StringRef getArgument() const override { return "ring-buffer"; }
  ::llvm::StringRef getDescription() const override {
    return "Converts SDF dataflow to a runtime with ring buffers.";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("RingBufferSchedulerPass");
  }

  Impl *pimpl;

  ::llvm::StringRef getName() const override {
    return "ringbufferSchedulerPass";
  }

  void runOnOperation() final override;
};

inline void registerRingBufferSchedulerPass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<
        iara::passes::ringbuffer::RingBufferSchedulerPass>();
  });
}

} // namespace iara::passes::ringbuffer

#endif
