//===- IaraOps.h - Iara dialect ops -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_IARAOPS_H
#define IARA_IARAOPS_H

// Suppress spurious "unused includes" diagnostics (compilers / linters vary).
// Place this before the include list in the header. If desired, restore
// diagnostics after the includes with a matching diagnostic pop.

#include "Iara/Dialect/AttrAccessor.h"                   // IWYU pragma: keep
#include "Iara/Dialect/IaraDialect.h"                    // IWYU pragma: keep
#include "Iara/Util/CommonTypes.h"                       // IWYU pragma: keep
#include "Iara/Util/CompilerTypes.h"                     // IWYU pragma: keep
#include "Iara/Util/rational.h"                          // IWYU pragma: keep
#include "mlir/Bytecode/BytecodeOpInterface.h"           // IWYU pragma: keep
#include "mlir/Dialect/Bufferization/IR/Bufferization.h" // IWYU pragma: keep
#include "mlir/Dialect/Func/IR/FuncOps.h"                // IWYU pragma: keep
#include "mlir/IR/BuiltinAttributes.h"                   // IWYU pragma: keep
#include "mlir/IR/BuiltinTypes.h"                        // IWYU pragma: keep
#include "mlir/IR/Dialect.h"                             // IWYU pragma: keep
#include "mlir/IR/OpDefinition.h"                        // IWYU pragma: keep
#include "mlir/IR/Operation.h"                           // IWYU pragma: keep
#include "mlir/IR/RegionKindInterface.h"                 // IWYU pragma: keep
#include "mlir/IR/SymbolTable.h"                         // IWYU pragma: keep
#include "mlir/Interfaces/InferTypeOpInterface.h"        // IWYU pragma: keep
#include "mlir/Interfaces/SideEffectInterfaces.h"        // IWYU pragma: keep

#define GET_OP_CLASSES
#include "Iara/Dialect/IaraOps.h.inc"

namespace iara {
using namespace mlir;
using namespace dialect;

// Follow an inout edge backwards, or return null.
EdgeOp followInoutChainBackwards(EdgeOp edge);

// Follow an inout edge forwards, or return null.
EdgeOp followInoutChainForwards(EdgeOp edge);

EdgeOp findFirstEdgeOfChain(EdgeOp edge);
EdgeOp findLastEdgeOfChain(EdgeOp edge);
i64 getDelaySizeBytes(EdgeOp edge);

struct InoutPair {
  Value in;
  Value out;
};

Vec<InoutPair> getInoutPairs(NodeOp node);

util::Rational getFlowRatio(EdgeOp edge);
i64 getProdRateBytes(EdgeOp edge);
i64 getConsRateBytes(EdgeOp edge);
NodeOp getProducerNode(EdgeOp edge);
NodeOp getConsumerNode(EdgeOp edge);

} // namespace iara

#endif // IARA_IARAOPS_H
