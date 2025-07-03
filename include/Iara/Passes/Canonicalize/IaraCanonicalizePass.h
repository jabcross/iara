#ifndef IARA_PASSES_CANONICALIZE_IARACANONICALIZEPASS_H
#define IARA_PASSES_CANONICALIZE_IARACANONICALIZEPASS_H

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

namespace iara::passes::canonicalize {

struct IaraCanonicalizePass
    : public PassWrapper<IaraCanonicalizePass,
                         OperationPass<::mlir::ModuleOp>> {

  IaraCanonicalizePass() = default;
  IaraCanonicalizePass(const IaraCanonicalizePass &pass) {};

  struct Impl;
  ::llvm::StringRef getArgument() const override { return "iara-canonicalize"; }
  ::llvm::StringRef getDescription() const override {
    return "Canonicalizes types, inserts implicit edges and broadcasts";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("IaraCanonicalizeSchedulerPass");
  }

  Impl *pimpl;

  ::llvm::StringRef getName() const override { return "IaraCanonicalizePass"; }

  void runOnOperation() final override;
};

inline void registerIaraCanonicalizePass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<iara::passes::canonicalize::IaraCanonicalizePass>();
  });
}

} // namespace iara::passes::canonicalize

#endif
