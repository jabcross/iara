#ifndef IARA_SCHEDULE_OPENMPSCHEDULER_H
#define IARA_SCHEDULE_OPENMPSCHEDULER_H

#include "Iara/IaraOps.h"
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Support/LogicalResult.h>

namespace mlir::iara {

struct OpenMPScheduler {
  func::FuncOp convertIntoOpenMP(ActorOp actor);
};
} // namespace mlir::iara

#endif // IARA_SCHEDULE_OPENMPSCHEDULER_H