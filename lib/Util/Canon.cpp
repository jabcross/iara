#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include <Util/Canon.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/Support/LogicalResult.h>

namespace mlir::iara::canon {

using namespace mlir::iara::mlir_util;
using namespace mlir::iara::rangeutil;

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
  return llvm::formatv("iara_broadcast_{0}x{1}", size,
                       mlir_util::stringifyType(type));
}

func::FuncOp getOrCodegenBroadcastImpl(Value value, i64 size) {
  std::string name = getBroadcastName(size, value.getType());

  // check if exists

  auto module = value.getDefiningOp()->getParentOfType<ModuleOp>();

  if (auto existing = module.lookupSymbol(name)) {
    return dyn_cast<func::FuncOp>(existing);
  }

  auto module_builder = OpBuilder(module);

  module_builder.setInsertionPointToStart(module.getBody());

  SmallVector<Type> input_types, output_types;

  for (auto i = 0; i < size; i++) {
    input_types.push_back(
        LLVM::LLVMPointerType::get(module_builder.getContext()));
  }

  auto impl =
      CREATE(func::FuncOp, module_builder, value.getDefiningOp()->getLoc(),
             name, module_builder.getFunctionType(input_types, output_types));

  auto body = impl.addEntryBlock();
  auto impl_builder = OpBuilder(impl);
  impl_builder.setInsertionPointToStart(body);

  auto size_val = mlir_util::getIntConstant(&impl.getFunctionBody().front(),
                                            (size_t)size * getTypeSize(value));

  auto src = body->getArgument(0);

  for (auto i : body->getArguments().drop_front(1)) {
    CREATE(LLVM::MemcpyOp, impl_builder, impl->getLoc(), i, src, size_val,
           false);
  }

  CREATE(func::ReturnOp, impl_builder, impl->getLoc(), {});

  return impl;
}

LogicalResult expandToBroadcast(OpResult &value) {
  auto module = value.getDefiningOp()->getParentOfType<ModuleOp>();
  OpBuilder builder(value.getOwner());
  builder.setInsertionPointAfter(value.getOwner());

  ValueRange in = {};
  ValueRange inout = {value};

  // SmallVector<Type> outputs;

  auto uses = value.getUses() | Pointers() | IntoVector();
  assert(uses.size() >= 2);
  auto output_types =
      uses | Map{[](auto &x) { return x->get().getType(); }} | IntoVector();

  auto impl = getOrCodegenBroadcastImpl(value, uses.size());

  assert(impl);

  auto broadcast_op = CREATE(NodeOp, builder, value.getOwner()->getLoc(),
                             output_types, impl.getSymName(), {}, in, inout);

  for (auto [new_value, operand] :
       llvm::zip_equal(broadcast_op.getOut(), uses)) {
    operand->set(new_value);
  }

  assert((value.getUses() | Pointers() | Count()) == 1);
  return success();
}

LogicalResult expandImplicitEdges(ActorOp actor) {
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
            CREATE(EdgeOp, builder,
                   builder.getFusedLoc({node.getLoc(), consumer_node.getLoc()}),
                   output.getType(), output);
        output.replaceAllUsesExcept(new_edge.getOut(), {new_edge});
      }
    }
  }
  return success();
}

LogicalResult canonicalize(ActorOp actor) { return expandImplicitEdges(actor); }

} // namespace mlir::iara::canon