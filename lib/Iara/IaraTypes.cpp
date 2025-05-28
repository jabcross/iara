//===- IaraTypes.cpp - Iara dialect types -----------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/Dialect/IaraTypes.h"

#include "Iara/Dialect/IaraDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

#define GET_TYPEDEF_CLASSES
#include "Iara/Dialect/IaraOpsTypes.cpp.inc"

void iara::dialect::IaraDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "Iara/Dialect/IaraOpsTypes.cpp.inc"
      >();
}
