#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include <llvm/Support/FormatVariadic.h>
#include <mlir/IR/Builders.h>
#include <numeric>

namespace iara::dialect::broadcast {

using namespace iara::util::mlir;
using namespace iara::util::range;

std::string getBroadcastName(i64 size, Type type, bool reuse_first) {
  if (reuse_first && size == 1) {
    llvm_unreachable("Pointless node");
  }
  auto type_string = stringifyType(type);
  if (reuse_first)
    return llvm::formatv("iara_broadcast_r_{0}x{1}", size, type_string);
  if (size == 1)
    return llvm::formatv("iara_copy_{0}", type_string);
  return llvm::formatv("iara_broadcast_{0}x{1}", size, type_string);
}

LLVM::LLVMFuncOp
getOrCodegenBroadcastImpl(Value value, i64 size, bool reuse_first) {
  std::string name = getBroadcastName(size, value.getType(), reuse_first);

  // check if exists

  auto module = value.getDefiningOp()->getParentOfType<ModuleOp>();

  // broken?
  // if (auto existing = module.lookupSymbol(name)) {
  //   return dyn_cast<func::FuncOp>(existing);
  // }

  for (auto &op : module.getOps()) {
    auto f = llvm::dyn_cast<LLVM::LLVMFuncOp>(&op);
    if (!f)
      continue;
    if (f.getSymName() == name) {
      return f;
    }
  }

  auto module_builder = OpBuilder(module);

  module_builder.setInsertionPointToStart(module.getBody());

  SmallVector<Type> arg_types;

  auto opaque_ptr = LLVM::LLVMPointerType::get(module_builder.getContext());

  auto num_args = size;

  if (reuse_first == false) {
    num_args++;
  }

  for (auto i = 0; i < num_args; i++) {
    arg_types.push_back(opaque_ptr);
  }

  auto impl = CREATE(
      LLVM::LLVMFuncOp,
      module_builder,
      value.getDefiningOp()->getLoc(),
      name,
      LLVM::LLVMFunctionType::get(
          LLVM::LLVMVoidType::get(module_builder.getContext()), arg_types));
  impl.setVisibility(mlir::SymbolTable::Visibility::Public);
  impl->setAttr("llvm.emit_c_interface", module_builder.getUnitAttr());

  auto body = impl.addEntryBlock(module_builder);
  auto impl_builder = OpBuilder(impl);
  impl_builder.setInsertionPointToStart(body);

  auto size_val =
      getIntConstant(&impl.getFunctionBody().front(), getTypeSize(value));

  auto src = body->getArgument(0);

  for (auto i : body->getArguments().drop_front(1)) {
    CREATE(
        LLVM::MemcpyOp, impl_builder, impl->getLoc(), i, src, size_val, true);
  }

  CREATE(LLVM::ReturnOp, impl_builder, impl->getLoc(), ValueRange{});

  return impl;
}

LogicalResult expandToBroadcast(OpResult &value) {
  OpBuilder builder(value.getOwner());
  builder.setInsertionPointAfter(value.getOwner());

  ValueRange in = {};
  Vec<Value> inout = {value};

  // SmallVector<Type> outputs;

  // getUses does not return users in topological order, for some reason.
  // While it technically doesn't matter, it's nice for debugging.
  auto uses = value.getUses() | Pointers() | IntoVector();
  Vec<OpOperand *> ordered_uses;

  for (auto &op : value.getOwner()->getParentRegion()->getOps()) {
    for (auto use : uses) {
      if (use->getOwner() != &op)
        continue;
      ordered_uses.push_back(use);
      break;
    }
  }

  assert(ordered_uses.size() == uses.size());

  // for (auto use : ordered_uses) {
  //   use->getOwner()->dump();
  // }

  auto output_types =
      uses | Map{[](auto &x) { return x->get().getType(); }} | IntoVector();

  auto impl = getOrCodegenBroadcastImpl(value, uses.size(), true);

  assert(impl);

  auto broadcast_op = CREATE(NodeOp,
                             builder,
                             value.getOwner()->getLoc(),
                             output_types,
                             impl.getSymName(),
                             {},
                             in,
                             inout);

  for (auto [new_value, operand] :
       llvm::zip_equal(broadcast_op.getOut(), uses)) {

    // llvm::errs() << "Operand index: " << operand->getOperandNumber() << "\n";
    // llvm::errs() << "Result index: " << new_value.getResultNumber() << "\n";
    // operand->getOwner()->dump();
    operand->set(new_value);
  }

  assert((value.getUses() | Pointers() | Count()) == 1);
  return success();
}

} // namespace iara::dialect::broadcast
