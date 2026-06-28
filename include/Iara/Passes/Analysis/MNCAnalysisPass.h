#ifndef IARA_PASSES_ANALYSIS_MNCANALYSISPASS_H
#define IARA_PASSES_ANALYSIS_MNCANALYSISPASS_H

#include "Iara/Dialect/IaraOps.h"
#include <llvm/Support/CommandLine.h>
#include <mlir/Pass/Pass.h>

namespace iara::passes::analysis {

struct MNCAnalysisPass
    : public mlir::PassWrapper<MNCAnalysisPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
  MNCAnalysisPass() = default;
  MNCAnalysisPass(const MNCAnalysisPass &) {}

  Option<std::string> strategy{
      *this, "strategy",
      llvm::cl::desc("MNC (max node concurrency) estimation strategy: "
                     "crude|antichain (env IARA_MNC_STRATEGY; default crude)")};

  ::llvm::StringRef getArgument() const override { return "iara-mnc"; }
  ::llvm::StringRef getDescription() const override {
    return "Estimates max node concurrency (MNC) and annotates each iara.node "
           "with an `iara.mnc` integer attribute.";
  }
  void runOnOperation() override;
};

inline void registerMNCAnalysisPass() {
  mlir::registerPass([]() -> std::unique_ptr<mlir::Pass> {
    return std::make_unique<MNCAnalysisPass>();
  });
}

} // namespace iara::passes::analysis

#endif
