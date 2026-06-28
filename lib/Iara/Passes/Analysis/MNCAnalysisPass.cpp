#include "Iara/Passes/Analysis/MNCAnalysisPass.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/EnvOption.h"
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/Builders.h>

using namespace iara::dialect;

namespace iara::passes::analysis {

void MNCAnalysisPass::runOnOperation() {
  auto module = getOperation();
  std::string strat = iara::util::optionOrEnv(
      strategy.hasValue(), strategy.getValue(), "IARA_MNC_STRATEGY", "crude");
  int64_t mnc;
  if (strat == "crude") {
    mnc = 1; // MOCK placeholder; real impl = max antichain over firing poset.
  } else if (strat == "antichain") {
    mnc = -1; // TODO(antichain): bipartite-matching max-antichain per node.
  } else {
    llvm::errs() << "Unknown MNC strategy '" << strat << "', using crude\n";
    mnc = 1;
  }
  auto i64 = mlir::IntegerType::get(&getContext(), 64);
  module.walk([&](NodeOp node) {
    node->setAttr("iara.mnc", mlir::IntegerAttr::get(i64, mnc));
  });
}

} // namespace iara::passes::analysis
