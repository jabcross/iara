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
include "mlir/IR/RegionKindInterface.td"
include "mlir/IR/BuiltinTypes.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/CommonAttrConstraints.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/BuiltinDialect.td"
include "mlir/IR/BuiltinTypeInterfaces.td"
include "mlir/IR/OpBase.td"
include "mlir/Dialect/Bufferization/IR/BufferizableOpInterface.td"

def ActorOp : IaraOp<"actor", [Symbol, SingleBlock, NoTerminator]> {
    let summary = "A dataflow actor. Defines an interface. May define a subgraph.";
    let description = [{
        The `iara.graph` operation is a container for nodes. It has the same
        interface as a kernel operation (in/out nodes, parameters).

        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins SymbolNameAttr:$sym_name, OptionalAttr<TypeArrayAttr>: $params);
    let regions = (region AnyRegion:$body);
    let assemblyFormat = [{ $sym_name ( `<` $params^ `>` )? $body attr-dict }];

    let extraClassDeclaration = [{
        bool isFlat();
      ::llvm::SmallVector<::mlir::Type> getParameterTypes();
      ::llvm::SmallVector<::mlir::Type> getAllInputTypes();
      ::llvm::SmallVector<::mlir::Type> getPureInTypes();
      ::llvm::SmallVector<::mlir::Type> getInoutTypes();
      ::llvm::SmallVector<::mlir::Type> getPureOutTypes();
      ::llvm::SmallVector<::mlir::Type> getAllOutputTypes();
      // is the input indexed by `idx` inout?
      bool isInoutInput(size_t idx);
      mlir::FunctionType getImplFunctionType();
      bool isKernel();
    }];
}

def NodeOp : IaraOp<"node", [AttrSizedOperandSegments,DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
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
    (`out` `:` type($out)^ )?
    attr-dict
  }];

    let extraClassDeclaration = [{
        bool isFlat();
        auto getInoutPairs(){
            return llvm::zip_first(getInout(), getResults());
        }
        auto getPureOuts(){
            return getResults().drop_front(getInout().size());
        }
        bool isInoutInput(OpOperand &operand);
        bool isPureInInput(OpOperand &operand);
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
    let arguments = (ins Variadic<Index>:$dyn_sizes, UnitAttr:$inout);
    let results = (outs AnyType:$result);
    let assemblyFormat = [{ (`inout` $inout^ )? (`<`$dyn_sizes^`>`)? `:` type($result) attr-dict }];
}

def OutPortOp: IaraOp<"out",  [AttrSizedOperandSegments] > {
    let summary = "An output port for a dataflow node.";
    let description = [{
        The `iara.out` operation declares an output port for a dataflow actor. It takes in a (possibly dynamic) tensor type and a list of dynamic sizes.
        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins Variadic<Index>:$dyn_sizes, TypeAttr:$type, Optional<AnyType>: $value);
    let assemblyFormat = [{  (`(` $value^ `:` type($value) `)`  )? ( `<` $dyn_sizes^ `>`)? `:` $type attr-dict }];
}

def DepOp: IaraOp<"dep", [Terminator]> {
    let summary = "A logical dependency block";
    let description = [{
        The `iara.dep` operation terminates a block and declares which blocks depend on this block.
        Example:

        ```mlir
        TODO
        ```
    }];
    let successors = (successor VariadicSuccessor<AnySuccessor>:$deps);
    let assemblyFormat = [{ $deps  attr-dict }];
}

// def NodeOp : IaraOp<"node", [AttrSizedOperandSegments, DeclareOpInterfaceMethods<SymbolUserOpInterface>] > {


//     let arguments = (ins FlatSymbolRefAttr:$impl, Variadic<AnyType>:$params, Variadic<AnyType> :$in, Variadic<AnyType> :$i_inout);
//     let results = (outs Variadic<AnyType>:$out, Variadic<AnyType> :$o_inout);

//     let assemblyFormat = [{ $impl oilist( `params` $params | `in`  $in | `inout`  $inout ) attr-dict }];

// }

#endif // IARA_OPS