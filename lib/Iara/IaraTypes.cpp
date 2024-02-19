//===- IaraTypes.cpp - Iara dialect types -----------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/IaraTypes.h"

#include "Iara/IaraDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir::iara;

#define GET_TYPEDEF_CLASSES
#include "Iara/IaraOpsTypes.cpp.inc"

void IaraDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "Iara/IaraOpsTypes.cpp.inc"
      >();
}
