#include "Iara/Schedule/OpenMPScheduler.h"
#include "Iara/Schedule/Schedule.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/ExecutionEngine/CRunnerUtils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir::iara {
std::unique_ptr<OpenMPScheduler> OpenMPScheduler::create(GraphOp graph) {
  auto rv = std::make_unique<OpenMPScheduler>();
  rv->m_graph = graph;
  return rv;
}

func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc) {
  auto rv =
      builder.create<func::FuncOp>(loc, name, builder.getFunctionType({}, {}));
  builder.atBlockEnd(rv.addEntryBlock()).create<func::ReturnOp>(loc);
  return rv;
}

LogicalResult OpenMPScheduler::emit(ModuleOp module) {
  OpBuilder builder{module};
  builder = builder.atBlockEnd(m_graph->getBlock());
  m_run_func = createEmptyVoidFunctionWithBody(builder, "__iara_run__",
                                               m_graph->getLoc());
  m_init_func = createEmptyVoidFunctionWithBody(builder, "__iara_init__",
                                                m_graph->getLoc());

  return success();
}

LogicalResult OpenMPScheduler::schedule() { return failure(); }

} // namespace mlir::iara