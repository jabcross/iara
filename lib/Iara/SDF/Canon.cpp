#include "Iara/Dialect/IaraOps.h"
#include "Iara/SDF/Canon.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include <llvm/Support/Casting.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Dominance.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::sdf::canon {

using namespace iara::util::mlir;
using namespace iara::util::range;

LogicalResult canonicalizeTypes(ActorOp actor) {
  for (Operation &op_ref : actor.getOps()) {
    auto op = &op_ref;
    if (!llvm::isa<NodeOp>(op) and !llvm::isa<EdgeOp>(op))
      continue;
    for (auto result : op->getResults()) {
      if (not dyn_cast<RankedTensorType>(result.getType())) {
        auto new_type = RankedTensorType::get({1}, result.getType());
        result.setType(new_type);
      }
    }
  }
  return success();
}

std::string getBroadcastName(i64 size, Type type) {
  return llvm::formatv("iara_broadcast_{0}x{1}", size, stringifyType(type));
}

LLVM::LLVMFuncOp getOrCodegenBroadcastImpl(Value value, i64 size) {
  std::string name = getBroadcastName(size, value.getType());

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

  for (auto i = 0; i < size; i++) {
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

  auto body = impl.addEntryBlock();
  auto impl_builder = OpBuilder(impl);
  impl_builder.setInsertionPointToStart(body);

  auto size_val = getIntConstant(&impl.getFunctionBody().front(),
                                 (size_t)size * getTypeSize(value));

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
  ValueRange inout = {value};

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

  for (auto use : ordered_uses) {
    use->getOwner()->dump();
  }

  auto output_types =
      uses | Map{[](auto &x) { return x->get().getType(); }} | IntoVector();

  auto impl = getOrCodegenBroadcastImpl(value, uses.size());

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

    llvm::errs() << "Operand index: " << operand->getOperandNumber() << "\n";
    llvm::errs() << "Result index: " << new_value.getResultNumber() << "\n";
    operand->getOwner()->dump();
    operand->set(new_value);
  }

  assert((value.getUses() | Pointers() | Count()) == 1);
  return success();
}

LogicalResult expandImplicitEdgesAndBroadcasts(ActorOp actor) {
  // First, expand all broadcasts.
  for (Operation *op : actor.getOps() | Pointers() | IntoVector()) {
    for (auto result : op->getResults()) {
      auto uses = result.getUses() | Pointers() | IntoVector();
      if (uses.size() > 1) {
        auto _ = expandToBroadcast(result);
      }
    }
  }

  // Insert edges between adjacent nodes.
  for (auto node : actor.getOps<NodeOp>() | IntoVector()) {
    for (auto output : node.getOut()) {
      auto uses = output.getUses() | Pointers() | IntoVector();
      auto users = output.getUsers() | IntoVector();
      if (uses.size() == 0) {
        node->emitError() << "Found an output port with no users: " << node
                          << "\n"
                          << "This is not supported in this version yet.";
        return failure();
      }
      assert((output.getUses() | Pointers() | Count()) == 1);
      if (auto consumer_node = dyn_cast<NodeOp>(users.front())) {
        auto builder = OpBuilder(consumer_node);
        auto new_edge =
            CREATE(EdgeOp,
                   builder,
                   builder.getFusedLoc({node.getLoc(), consumer_node.getLoc()}),
                   output.getType(),
                   output);
        output.replaceAllUsesExcept(new_edge.getOut(), {new_edge});
      }
    }
  }
  return success();
}

LogicalResult canonicalize(ActorOp actor) {
  return expandImplicitEdgesAndBroadcasts(actor);
  actor.dump();
}

} // namespace iara::sdf::canon
