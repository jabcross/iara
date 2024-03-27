#include "Iara/Schedule/Schedule.h"
#include "mlir/IR/TypeRange.h"
#include "util/util.h"
#include "llvm/ADT/STLExtras.h"
#include <llvm/Support/YAMLParser.h>
#include <mlir/IR/Value.h>

namespace mlir::iara {
bool Scheduler::checkSingleRate() {
  assert(m_graph.getParameterTypes().empty() &&
         m_graph->getOperands().empty() && m_graph.isFlat());

  // check that all nodes have a single rate

  assert(llvm::all_of(m_graph.getOps<NodeOp>(),
                      [](NodeOp node) { return node.getParams().empty(); }));

  // todo: check all port connections
  return true;
}

} // namespace mlir::iara
