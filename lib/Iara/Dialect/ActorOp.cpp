#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"

namespace iara {

using namespace util::range;

bool ActorOp::isEmpty() {
  for (Operation &op : this->getOps()) {
    if (llvm::isa<NodeOp, EdgeOp>(&op))
      return false;
  }
  return true;
}

llvm::SmallVector<Type> ActorOp::getParameterTypes() {
  return getParams() | Map([](auto param) {
           return llvm::cast<TypeAttr>(param).getValue();
         }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getAllInputTypes() {
  return getOps<InPortOp>() |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getPureInTypes() {
  return getOps<InPortOp>() | Filter([](auto op) { return !op.getInout(); }) |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getInoutTypes() {
  return getOps<InPortOp>() | Filter([](auto op) { return op.getInout(); }) |
         Map([](auto op) { return op.getResult().getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getPureOutTypes() {
  return getOps<OutPortOp>() | Drop(getInoutTypes().size()) |
         Map([](auto op) { return op.getOperand(0).getType(); }) |
         Into<SmallVector<Type>>();
};
llvm::SmallVector<Type> ActorOp::getAllOutputTypes() {
  return getOps<OutPortOp>() |
         Map([](OutPortOp op) { return op.getValue().getType(); }) |
         Into<SmallVector<Type>>();
};
bool ActorOp::isInoutInput(size_t idx) {
  return idx >= getPureInTypes().size();
}
bool ActorOp::isKernelDeclaration() {
  if ((*this)->hasAttr("kernel-decl"))
    return true;
  if ((*this)->hasAttr("kernel-def"))
    return false;
  auto nodes = (*this).getOps<NodeOp>() | IntoVector();
  if (nodes.empty()) {
    (*this)->setAttr("kernel-decl", OpBuilder(*this).getUnitAttr());
    return true;
  }
  (*this)->setAttr("kernel-def", OpBuilder(*this).getUnitAttr());
  return false;
}

mlir::FunctionType ActorOp::getImplFunctionType() {
  OpBuilder builder{*this};
  SmallVector<Type> tensor_types;
  tensor_types.append(getParameterTypes());
  tensor_types.append(getPureInTypes());
  tensor_types.append(getAllOutputTypes());
  for (auto &type : tensor_types) {
    if (!isa<TensorType>(type))
      type = RankedTensorType::get({1}, type);
  }

  assert(llvm::all_of(tensor_types, [](Type t) { return isa<TensorType>(t); }));
  auto memref_types = tensor_types | Map([](auto type) {
                        auto tensor_type = cast<TensorType>(type);
                        return MemRefType::get(tensor_type.getShape(),
                                               tensor_type.getElementType());
                      }) |
                      Into<SmallVector<Type>>();
  return builder.getFunctionType(memref_types, TypeRange{});
}

LogicalResult ActorOp::verifyRegions() {
  int inouts = 0;
  int outs = 0;
  for (auto &op : this->getOps()) {
    if (auto in = dyn_cast<InPortOp>(&op); in && in.getInout())
      inouts++;
    if (isa<OutPortOp>(&op))
      outs++;
  }
  if (inouts > outs)
    return failure();
  return success();
}

bool ActorOp::hasInterface() {
  for (auto _ : getParams()) {
    return true;
  }
  for (Operation &op : getOps()) {
    if (isa<InPortOp>(&op) or isa<OutPortOp>(&op)) {
      return true;
    }
  }
  return false;
}
}