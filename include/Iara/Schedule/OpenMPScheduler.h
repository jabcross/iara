#ifndef IARA_SCHEDULE_OPENMPSCHEDULER_H
#define IARA_SCHEDULE_OPENMPSCHEDULER_H

#include "Iara/IaraOps.h"
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Support/LogicalResult.h>

namespace mlir::iara {

class OpenMPScheduler {
public:
  ModuleOp m_module;
  ActorOp m_graph;
  func::FuncOp m_run_func;
  func::FuncOp m_init_func;

  static std::unique_ptr<OpenMPScheduler> create(ActorOp graph);
  bool checkSingleRate();
  LogicalResult convertToTasks();
  LogicalResult convertIntoOpenMP();
  LogicalResult convertIntoSequential();
};
} // namespace mlir::iara

#endif // IARA_SCHEDULE_OPENMPSCHEDULER_H