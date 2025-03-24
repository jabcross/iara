//===- IaraDialect.h - Iara dialect -----------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef IARA_IARADIALECT_H
#define IARA_IARADIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/Dialect.h"
#include <llvm/ADT/StringRef.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/Operation.h>

namespace mlir::iara {

template <typename ConcreteType>
class IaraOp : public mlir::OpTrait::TraitBase<ConcreteType, IaraOp> {
public:
  class AttrAccessor {
  public:
    Operation *op;
    StringRef attr_name;

    Attribute get() { return op->getAttr(attr_name); }

    void operator=(AttrAccessor) = delete;
    void operator=(AttrAccessor &) = delete;
    void operator=(AttrAccessor &&) = delete;

    IntegerAttr operator=(int64_t value) {
      auto attr = OpBuilder{op}.getI64IntegerAttr(value);
      op->setAttr(attr_name, attr);
      return attr;
    }

    Attribute operator=(Attribute value) {
      op->setAttr(attr_name, value);
      return value;
    }

    operator Attribute() { return get(); }
    explicit operator int64_t() {
      return op->getAttrOfType<IntegerAttr>(attr_name).getInt();
    };
    Attribute operator*() { return get(); }
  };

  AttrAccessor operator[](StringRef attr_name) {
    return AttrAccessor{this->getOperation(), attr_name};
  };

  void traitMethod(){};
};

} // namespace mlir::iara

#include "Iara/IaraOpsDialect.h.inc"

#endif // IARA_IARADIALECT_H
