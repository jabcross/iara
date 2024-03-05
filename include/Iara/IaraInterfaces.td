#ifndef IARA_INTERFACES
#define IARA_INTERFACES

include "mlir/IR/OpBase.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/Interfaces.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/IR/Interfaces.td"
include "Iara/IaraDialect.td"

def ActorInterface : OpInterface<"ActorInterface", [Symbol]> {
    let cppNamespace = "::mlir::iara";
    let description = [{
      Interface for an entity that can act as an actor
      implementation. Defines parameters and ports.";
    }];

    // let methods = [
    //   InterfaceMethod<"getParameters", () -> ArrayAttr>,
    //   InterfaceMethod<"getInPorts", () -> ArrayAttr>,
    //   InterfaceMethod<"getOutPorts", () -> ArrayAttr>,
    //   InterfaceMethod<"getInoutPorts", () -> ArrayAttr>,
    //   InterfaceMethod<"getInoutPorts", () -> ArrayAttr>,
    // ];

}

#endif // IARA_INTERFACES