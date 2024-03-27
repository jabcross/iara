#ifndef IARA_SCHEDULE_OPENMPSCHEDULER_H
#define IARA_SCHEDULE_OPENMPSCHEDULER_H

#include "Iara/IaraOps.h"
#include "Iara/Schedule/Schedule.h"

namespace mlir::iara {

class OpenMPScheduler : public Scheduler {
public:
  virtual ~OpenMPScheduler() = default;
  static std::unique_ptr<OpenMPScheduler> create(ActorOp graph);
  virtual LogicalResult emit(ModuleOp module) override;
  virtual LogicalResult schedule() override;
};
} // namespace mlir::iara

#endif // IARA_SCHEDULE_OPENMPSCHEDULER_H