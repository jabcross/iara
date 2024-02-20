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

class Iara_Type<string name, string typeMnemonic, list<Trait> traits = []>
    : TypeDef<Iara_Dialect, name, traits> {
  let mnemonic = typeMnemonic;
}

def Iara_CustomType : Iara_Type<"Custom", "custom"> {
    let summary = "Iara custom type";
    let description = "Custom type in iara dialect";
    let parameters = (ins StringRefParameter<"the custom value">:$value);
    let assemblyFormat = "`<` $value `>`";
}

def Fifo : Iara_Type<"Fifo", "fifo"> {
  let summary = "Fifo type";
  let description = "Fifo type in iara dialect";
  let parameters = (ins TypeParameter<"the datatype">:$datatype);
  let assemblyFormat = "`fifo<` $datatype `>`";
}

#endif // IARA_TYPES
