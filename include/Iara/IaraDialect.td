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

def Iara_Dialect : Dialect {
    let name = "iara";
    let summary = "A iara out-of-tree MLIR dialect.";
    let description = [{
        This dialect is an example of an out-of-tree MLIR dialect designed to
        illustrate the basic setup required to develop MLIR-based tools without
        working inside of the LLVM source tree.
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

class Iara_Op<string mnemonic, list<Trait> traits = []> :
        Op<Iara_Dialect, mnemonic, traits>;

#endif // IARA_DIALECT
