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

def ParameterMaterializePass: Pass<"iara-param-materialize", "::mlir::ModuleOp"> {
  let summary = "Materialize actor parameters into static tensor types.";
  let description = [{
    Runs after --flatten. Three phases:
      1. Replace the flat actor's block-argument parameters with arith.constant
         values read from its `default_params` attribute.
      2. Nested --sccp folds the arith computation chains (params feeding
         computed sizes) down to concrete arith.constant values.
      3. Resolve dynamic tensor types: InPortOp/OutPortOp with `dyn_sizes`
         operands (now folded constants) get static tensor types and empty
         dyn_sizes; EdgeOps rebuild to match their static inputs.
    After this the whole graph is statically typed for --virtual-fifo.
  }];
  let dependentDialects = ["::mlir::arith::ArithDialect"];
}


#endif // IARA_PASS