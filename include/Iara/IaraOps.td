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
include "Iara/IaraInterfaces.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Bytecode/BytecodeOpInterface.td"
include "mlir/IR/RegionKindInterface.td"
include "mlir/IR/BuiltinTypes.td"

def IaraFooOp : IaraOp<"foo", [Pure,
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

def GraphOp : IaraOp<"graph", [ActorInterface, Symbol, SingleBlock, NoTerminator, DeclareOpInterfaceMethods<ActorInterface>]> {
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

    let assemblyFormat = [{ $sym_name $body attr-dict }];
    // ActorInterface Methods

    let extraClassDeclaration = [{
        bool isFlat();
    }];
}

def KernelOp : IaraOp<"kernel", [ActorInterface, Symbol, SingleBlock , NoTerminator, DeclareOpInterfaceMethods<ActorInterface>] > {
    let summary = "A kernel for a dataflow node.";
    let description = [{
        The `iara.kernel` operation is a kernel for a dataflow node. It has
        the same interface as a graph operation (in/out nodes, parameters).

        The name should match a C function that implements the kernel,
        whose parameters are, in order, the kernel's parameters, the inputs, the inouts, and the outputs.

        The body should contain the kernel's ports.

        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins SymbolNameAttr:$sym_name, Variadic
    <AnyType>: $params);
    let regions = (region AnyRegion:$body);
    let assemblyFormat = [{ $sym_name ($params^ `:` type($params))? $body attr-dict }];

}

def NodeOp : IaraOp<"node", [AttrSizedOperandSegments, DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
    let summary = "A node in a dataflow graph.";
    let description = [{
        The `iara.node` operation is a node in a dataflow graph. It has a
        reference to an implementation, takes in parameters, in and inout edges,
        and returns out and inout edges.

        Example:

        ```mlir
        TODO
        ```
    }];
  let arguments = (ins FlatSymbolRefAttr:$impl,
                       Variadic<AnyType>:$params,
                       Variadic<AnyType>:$in,
                       Variadic<AnyType>:$inout);
    let results = (outs Variadic<AnyType>:$out);
  let assemblyFormat = [{
    $impl
    (`params` $params^ `:` type($params) )?
    (`in` $in^ `:` type($in))?
    (`inout` $inout^ `:` type($inout) )?
    `out` type($out)
    attr-dict
  }];

    let extraClassDeclaration = [{
        bool isFlat();
    }];

}

def InPortOp: IaraOp<"in", [] > {
    let summary = "An input port for a dataflow node.";
    let description = [{
        The `iara.in` operation declares an input port for a dataflow actor. It takes in a (possibly dynamic) tensor type and a list of dynamic sizes.
        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins Variadic<Index>:$dyn_sizes, TypeAttr:$type);
    let assemblyFormat = [{ ($dyn_sizes^)? $type attr-dict }];
}

def OutPortOp: IaraOp<"out", [] > {
    let summary = "An output port for a dataflow node.";
    let description = [{
        The `iara.out` operation declares an output port for a dataflow actor. It takes in a (possibly dynamic) tensor type and a list of dynamic sizes.
        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins Variadic<Index>:$dyn_sizes, TypeAttr:$type);
    let assemblyFormat = [{ ($dyn_sizes^)? $type attr-dict }];
}


// def NodeOp : IaraOp<"node", [AttrSizedOperandSegments, DeclareOpInterfaceMethods<SymbolUserOpInterface>] > {


//     let arguments = (ins FlatSymbolRefAttr:$impl, Variadic<AnyType>:$params, Variadic<AnyType> :$in, Variadic<AnyType> :$i_inout);
//     let results = (outs Variadic<AnyType>:$out, Variadic<AnyType> :$o_inout);

//     let assemblyFormat = [{ $impl oilist( `params` $params | `in`  $in | `inout`  $inout ) attr-dict }];

// }

#endif // IARA_OPS