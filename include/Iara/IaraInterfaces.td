#ifndef IARA_INTERFACES
#define IARA_INTERFACES

include "mlir/IR/AttrTypeBase.td"
include "Iara/IaraDialect.td"

//===----------------------------------------------------------------------===//
// Iara interface definitions
//===----------------------------------------------------------------------===//

def ActorInterface : OpInterface<"ActorInterface", [Symbol]> {
  let cppNamespace = "::mlir::iara";
    let description = "Interface for an entity that can act as an actor
      implementation. Defines parameters and ports.";
    let methods = [
      Method<"getParameters", () -> ArrayAttr>,
      Method<"getInPorts", () -> ArrayAttr>,
      Method<"getOutPorts", () -> ArrayAttr>,
      Method<"getInoutPorts", () -> ArrayAttr>,
      Method<"getInoutPorts", () -> ArrayAttr>,
    ];
}

#endif // IARA_INTERFACES
