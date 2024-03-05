//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/IaraOps.h"
#include "Iara/IaraDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/Support/Casting.h"

#define GET_OP_CLASSES
#include "Iara/IaraOps.cpp.inc"

namespace mlir::iara {
bool GraphOp::isFlat() {
  int interfaced_nodes = 0;
  this->walk([&](NodeOp node) {
    // get the name of the implementation
    auto impl_name = node.getImpl();
    // lookup in the containing module
    auto impl = node->getParentOfType<ModuleOp>().lookupSymbol(impl_name);
    if (isa<GraphOp>(impl)) {
      interfaced_nodes++;
      return WalkResult::interrupt();
    }
  });
  return interfaced_nodes == 0;
}
} // namespace mlir::iara