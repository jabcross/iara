//===- IaraOps.td - Iara dialect ops -----------*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_OPS
#define IARA_OPS

include "Iara/Dialect/IaraTypes.td"
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

def ActorOp : IaraOp<"actor", [Symbol, NoTerminator, RegionKindInterface, IsolatedFromAbove]> {
    let summary = "A dataflow actor. Defines an interface. May define a subgraph.";
    let description = [{
        The `iara.actor` operation is a container for nodes. It has the same
        interface as a kernel operation (in/out nodes, parameters).

        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins SymbolNameAttr:$sym_name, OptionalAttr<TypeArrayAttr>: $params);
    let regions = (region AnyRegion:$body);
    let assemblyFormat = [{ $sym_name ( `<` $params^ `>` )? $body attr-dict }];
    let hasRegionVerifier = 1;

    let extraClassDeclaration = [{
      bool isFlat() { return (*this)->hasAttr("flat"); }
      bool isInTaskForm() { return (*this)->hasAttr("task-form"); }
      bool isEmpty();
      bool isKernelDeclaration();
      bool hasInterface();
      ::llvm::SmallVector<::mlir::Type> getParameterTypes();
      ::llvm::SmallVector<::mlir::Type> getAllInputTypes();
      ::llvm::SmallVector<::mlir::Type> getPureInTypes();
      ::llvm::SmallVector<::mlir::Type> getInoutTypes();
      ::llvm::SmallVector<::mlir::Type> getPureOutTypes();
      ::llvm::SmallVector<::mlir::Type> getAllOutputTypes();
      // is the input indexed by `idx` inout?
      bool isInoutInput(size_t idx);
      mlir::FunctionType getImplFunctionType();
      static ::mlir::RegionKind getRegionKind(unsigned index) {
        return ::mlir::RegionKind::Graph;
      }
    }];
}

def NodeOp : IaraOp<"node", [IsolatedFromAbove, AttrSizedOperandSegments,DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
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

    // let assemblyFormat = [{
    //     $impl
    //     (`params` $params^ `:` type($params) )?
    //     (`in` $in^ `:` type($in))?
    //     (`inout` $inout^ `:` type($inout) )?
    //     (`out` `:` type($out)^ )?
    //     attr-dict
    // }];
    let hasCustomAssemblyFormat = 1;

    let extraClassDeclaration = [{
        bool isFlat();
        auto getPureOuts(){
            return getResults().drop_front(getInout().size());
        }
        bool isInoutInput(OpOperand &operand);
        bool isInoutOutput(OpResult &result);
        bool isPureInInput(OpOperand &operand);
        llvm::SmallVector<mlir::Value> getAllInputs();
        llvm::SmallVector<mlir::Value> getAllOutputs();
        Value getMatchingInoutInput(Value result);
        Value getMatchingInoutOutput(Value operand);
        func::CallOp convertToCallOp();
        FunctionType getKernelFunctionType();
        bool signatureMatches(ActorOp actor);
        bool isAlloc();
        bool isDealloc();
    }];



}

def EdgeOp : IaraOp<"edge", []> {
    let summary = [{An edge in a dataflow graph. Used to connect inputs and
        outputs of nodes when there is a type mismatch, or to add extra edge
        information.
        }];
    let description = [{
        The `iara.edge` operation is used for non-syncrhonous dataflow edges,
        when the tensor types do not match.

        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins AnyType:$in);
    let results = (outs AnyType:$out);
    let assemblyFormat = [{
      $in `:` type($in) `->` type($out)
      attr-dict
  }];
}

// def ParamOp: IaraOp<"param", [AllTypesMatch<["type", "result"]>] > {
//     let summary = "A parameter declaration.";
//     let description = [{
//         The `iara.param` operation declares a parameter for a dataflow actor. It takes in any type that can be lowered to a LLVM value.
//         Example:

//         ```mlir
//         TODO
//         ```
//     }];
//     let arguments = (ins TypeAttr:$type);
//     let results = (outs AnyType:$result);
//     let assemblyFormat = [{ $type attr-dict }];
// }

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

def OutPortOp: IaraOp<"out",  [] > {
    let summary = "An output port for a dataflow node.";
    let description = [{
        The `iara.out` operation declares an output port for a dataflow actor. It takes in a (possibly dynamic) tensor type and a list of dynamic sizes.
        Example:

        ```mlir
        TODO
        ```
    }];
    let arguments = (ins Variadic<Index>:$dyn_sizes, AnyType: $value);
    let assemblyFormat = [{  `(` $value `:` type($value) `)` ( `<` $dyn_sizes^ `>`)?  attr-dict }];
}


#endif // IARA_OPS
