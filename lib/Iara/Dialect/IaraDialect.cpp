//===- IaraDialect.cpp - Iara dialect ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/IaraTypes.h"

using namespace mlir;
using namespace iara;

#include "Iara/Dialect/IaraOpsDialect.cpp.inc"

//===----------------------------------------------------------------------===//
// Iara dialect.
//===----------------------------------------------------------------------===//

void IaraDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "Iara/Dialect/IaraOps.cpp.inc"
      >();
  registerTypes();
}
