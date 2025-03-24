#ifndef IARA_PASS
#define IARA_PASS

include "mlir/Pass/PassBase.td"

def FlattenPass: Pass<"flatten", "::mlir::ModuleOp"> {
  let summary = "Flatten hierarchy of the module.";
  let description = [{
    Find hierarchical dataflow operations in the module and flatten them by inlining their contents.
    Also removes kernel declarations (empty actor ops).
  }];
}


#endif // IARA_PASS