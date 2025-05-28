#ifndef IARA_SCHEDULE_OPENMPSCHEDULER_H
#define IARA_SCHEDULE_OPENMPSCHEDULER_H

#include "Iara/Dialect/IaraOps.h"
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Support/LogicalResult.h>

namespace iara {

struct OpenMPScheduler {
  func::FuncOp convertIntoOpenMP(DAGOp dag);
};
} // namespace iara

#endif // IARA_SCHEDULE_OPENMPSCHEDULER_H
