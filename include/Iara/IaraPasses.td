#ifndef IARA_PASS
#define IARA_PASS

include "mlir/Pass/PassBase.td"

def FlattenPass: Pass<"flatten", "::mlir::ModuleOp"> {
  let summary = "Flatten hierarchy of the module.";
  let description = [{
    Find hierarchical dataflow operations in the module and flatten them by inlining their contents.
  }];
}

def CollapseEdgesPass: Pass<"collapse-edges", "::mlir::ModuleOp"> {
  let summary = "Collapse edges in the module.";
  let description = [{
    Removes edges when the type is the same and there is no delay.
  }];
}

def LowerToTasksPass: Pass<"sdf-to-tasks", "::mlir::ModuleOp"> {
  let summary = "Converts SDF dataflow to task DAG, generating memory management code.";
  let description = [{
    Converts dataflow to task DAG, generating memory management code.
    Actor ops are converted to DAG ops.
  }];
}

def LowerToOpenMPPass: Pass<"tasks-to-openmp", "::mlir::ModuleOp"> {
  let summary = "Converts tasks to OpenMP, outputting upstream MLIR.";
  let description = [{
    Converts tasks to OpenMP, outputting upstream MLIR.
  }];
}

#endif // IARA_PASS