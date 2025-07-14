//===- IaraOps.cpp - Iara dialect ops ---------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/rational.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Casting.h"
#include <assert.h>
#include <functional>
#include <iterator>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/Bufferization/IR/BufferizableOpInterface.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>

#define GET_OP_CLASSES
#include "Iara/Dialect/IaraOps.cpp.inc"
