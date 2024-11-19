#ifndef IARA_PASS
#define IARA_PASS

include "mlir/Pass/PassBase.td"

def IaraFlatten: Pass<"iara-flatten", "::mlir::ModuleOp"> {
  let summary = "Flatten hierarchy of the module.";
  let description = [{
    Find hierarchical dataflow operations in the module and flatten them by inlining their contents.
    Should end with a single actor named "main".
  }];
}

def IaraCollapseEdges: Pass<"iara-collapse-edges", "::mlir::ModuleOp"> {
  let summary = "Collapse edges in the module.";
  let description = [{
    Removes edges when the type is the same and there is no delay.
  }];
}

def IaraSchedule: Pass<"iara-schedule", "::mlir::ModuleOp"> {
  let summary = "Schedule the dataflow operations in the module.";
  let description = [{
    Schedule the dataflow operations in the module. Outputs standard MLIR.
  }];
}

#endif // IARA_PASS