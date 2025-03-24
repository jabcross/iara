//===- IaraOps.h - Iara dialect ops -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_IARAOPS_H
#define IARA_IARAOPS_H

#include "Iara/IaraDialect.h"
#include "Util/rational.h"
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
#include "Iara/IaraOps.h.inc"

namespace mlir::iara {

// Follow an inout edge backwards, or return null.
EdgeOp followInoutEdgeBackwards(EdgeOp edge);

// Follow an inout edge forwards, or return null.
EdgeOp followInoutEdgeForwards(EdgeOp edge);

} // namespace mlir::iara

#endif // IARA_IARAOPS_H
