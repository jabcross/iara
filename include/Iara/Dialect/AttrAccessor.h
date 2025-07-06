#ifndef IARA_DIALECT_ATTR_ACCESSOR_H
#define IARA_DIALECT_ATTR_ACCESSOR_H

#include "Iara/Util/Mlir.h"
#include <mlir/IR/OpDefinition.h>

namespace iara::dialect {

class AttrAccessor {
public:
  Operation *op;
  StringRef attr_name;

  Attribute get() { return op->getAttr(attr_name); }

  void operator=(AttrAccessor) = delete;
  void operator=(AttrAccessor &) = delete;
  void operator=(AttrAccessor &&) = delete;

  IntegerAttr operator=(i64 value) {
    auto attr = OpBuilder{op}.getI64IntegerAttr(value);
    op->setAttr(attr_name, attr);
    return attr;
  }

  bool operator==(const char *string) {
    StringAttr attr = llvm::dyn_cast_if_present<StringAttr>(get());
    return attr && (attr.getValue() == string);
  };

  Attribute operator=(Attribute value) {
    op->setAttr(attr_name, value);
    return value;
  }

  operator bool() { return bool(op->getAttr(attr_name)); }

  explicit operator i64() {
    return op->getAttrOfType<IntegerAttr>(attr_name).getInt();
  };

  explicit operator StringRef() {
    return op->getAttrOfType<StringAttr>(attr_name).getValue();
  }

  operator Attribute() {
    if (op->hasAttr(attr_name))
      return op->getAttr(attr_name);
    return Attribute();
  }

  Attribute operator*() { return get(); }
};

template <typename ConcreteType>
class AttrAccessorTrait
    : public mlir::OpTrait::TraitBase<ConcreteType, AttrAccessorTrait> {
public:
  AttrAccessor operator[](StringRef attr_name) {
    return AttrAccessor{this->getOperation(), attr_name};
  };
};

} // namespace iara::dialect

#endif // IARA_DIALECT_ATTR_ACCESSOR_H