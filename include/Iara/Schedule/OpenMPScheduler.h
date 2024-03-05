#include "Iara/IaraOps.h"
#include "Iara/Schedule/Schedule.h"

namespace mlir::iara {

class OpenMPScheduler : public Scheduler {
  GraphOp m_graph = 0;
  func::FuncOp m_run_func = 0;
  func::FuncOp m_init_func = 0;

public:
  virtual ~OpenMPScheduler() = default;
  static std::unique_ptr<OpenMPScheduler> create(GraphOp graph);
  virtual LogicalResult emit(ModuleOp module) override;
  virtual LogicalResult schedule() override;
};
} // namespace mlir::iara