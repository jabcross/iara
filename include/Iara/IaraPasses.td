#ifndef IARA_PASS
#define IARA_PASS

include "mlir/Pass/PassBase.td"

def IaraSchedule: Pass<"iara-schedule", "::mlir::ModuleOp"> {
  let summary = "Schedule the dataflow operations in the module.";
  let description = [{
    Schedule the dataflow operations in the module. Outputs standard MLIR.
  }];
}

#endif // IARA_PASS