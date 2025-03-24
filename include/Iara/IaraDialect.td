//===- IaraDialect.td - Iara dialect -----------*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_DIALECT
#define IARA_DIALECT

include "mlir/IR/OpBase.td"

//===----------------------------------------------------------------------===//
// Iara dialect definition.
//===----------------------------------------------------------------------===//

def IaraDialect : Dialect {
    let name = "iara";
    let summary = "A dataflow dialect fot MLIR.";
    let description = [{
        This is a dialect for defining dataflow semantics inside of MLIR, with
        support for dataflow graphs, nodes, and edges, parameters, interfaces,
        in-place computaton, and more.
    }];
    let cppNamespace = "::mlir::iara";

    let useDefaultTypePrinterParser = 1;
    let extraClassDeclaration = [{
        void registerTypes();
    }];
}

//===----------------------------------------------------------------------===//
// Base iara operation definition.
//===----------------------------------------------------------------------===//


def IaraOpTrait: NativeOpTrait<"IaraOp"> {
    string cppNamespace = "::mlir::iara";
}

class IaraOp<string mnemonic, list<Trait> traits = []> :
        Op<IaraDialect, mnemonic, !listconcat([IaraOpTrait], traits)>;

#endif // IARA_DIALECT