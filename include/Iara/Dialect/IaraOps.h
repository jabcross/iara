//===- IaraOps.h - Iara dialect ops -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_IARAOPS_H
#define IARA_IARAOPS_H

#include "Iara/Dialect/AttrAccessor.h"
#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/rational.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/RegionKindInterface.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

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
