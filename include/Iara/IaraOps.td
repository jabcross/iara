//===- IaraOps.td - Iara dialect ops -----------*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_OPS
#define IARA_OPS

include "Iara/IaraTypes.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Bytecode/BytecodeOpInterface.td"

def Iara_FooOp : Iara_Op<"foo", [Pure,
                                             SameOperandsAndResultType]> {
    let summary = "Illustrates how to define an operation.";
    let description = [{
        The `iara.foo` operation illustrates how to define a new
        operation in a dialect. It uses an operation trait to declare that it
        has no side effects.

        This operation takes an integer argument and returns an integer.

        Example:

        ```mlir
        %0 = arith.constant 2 : i32
        // Apply the foo operation to %0
        %1 = iara.foo %0 : i32
        ```
    }];

    let arguments = (ins I32:$input);
    let results = (outs I32:$res);

    let assemblyFormat = [{
        $input attr-dict `:` type($input)
    }];
}

def Iara_GraphOp : Iara_Op<"graph", [Symbol]> {
    let summary = "A dataflow graph. Contains nodes.";
    let description = [{
        The `iara.graph` operation is a container for nodes. It has the same
        interface as a kernel operation (in/out nodes, parameters).

        Example:

        ```mlir
        TODO
        ```
    }];

    let arguments = (ins SymbolNameAttr:$sym_name);
    let regions = (region AnyRegion:$body);

    // ActorInterface Methods

}

#endif // IARA_OPS
