//===- IaraTypes.td - Iara dialect types -------*- tablegen -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_TYPES
#define IARA_TYPES

include "mlir/IR/AttrTypeBase.td"
include "Iara/IaraDialect.td"

//===----------------------------------------------------------------------===//
// Iara type definitions
//===----------------------------------------------------------------------===//

class IaraType<string name, string typeMnemonic, list<Trait> traits = []>
    : TypeDef<IaraDialect, name, traits> {
  let mnemonic = typeMnemonic;
}

def IaraCustomType : IaraType<"Custom", "custom"> {
    let summary = "Iara custom type";
    let description = "Custom type in iara dialect";
    let parameters = (ins StringRefParameter<"the custom value">:$value);
    let assemblyFormat = "`<` $value `>`";
}

def Fifo : IaraType<"Fifo", "fifo"> {
  let summary = "Fifo type";
  let description = "Fifo type in iara dialect";
  let parameters = (ins TypeParameter<"::mlir::Type","the datatype">:$datatype);
  let assemblyFormat = "`<` $datatype `>`";
}

#endif // IARA_TYPES
