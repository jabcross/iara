//===- IaraDialect.h - Iara dialect -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_IARADIALECT_H
#define IARA_IARADIALECT_H

#include "Util/MlirUtil.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/Dialect.h"
#include <cstring>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/Operation.h>

namespace mlir::iara {

template <typename ConcreteType>
class IaraOp : public mlir::OpTrait::TraitBase<ConcreteType, IaraOp> {
public:
  mlir_util::AttrAccessor operator[](StringRef attr_name) {
    return mlir_util::AttrAccessor{this->getOperation(), attr_name};
  };

  void traitMethod(){};
};

} // namespace mlir::iara

#include "Iara/IaraOpsDialect.h.inc"

#endif // IARA_IARADIALECT_H
