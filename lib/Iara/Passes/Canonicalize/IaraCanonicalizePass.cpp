#include "Iara/Dialect/Broadcast.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/Range.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Dominance.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::canonicalize {

using namespace iara::util::mlir;
using namespace iara::util::range;

void expandImplicitEdgesAndBroadcasts(ActorOp actor) {
  // First, expand all broadcasts.
  for (Operation *op : actor.getOps() | Pointers() | IntoVector()) {
    for (auto result : op->getResults()) {
      auto uses = result.getUses() | Pointers() | IntoVector();
      if (uses.size() > 1) {
        auto _ = iara::dialect::broadcast::expandToBroadcast(result);
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
        llvm_unreachable("Not implemented");
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
  return;
}

struct IaraCanonicalizePass::Impl {
  IaraCanonicalizePass *pass;

  Impl(IaraCanonicalizePass *pass) : pass(pass) {}

  void canonicalizeTypes(ActorOp actor) {
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
    return;
  }

  void canonicalizeActor(ActorOp actor) {
    canonicalizeTypes(actor);
    expandImplicitEdgesAndBroadcasts(actor);
  }

  LogicalResult runOnOperation(ModuleOp module) {
    for (auto actor : module.getOps<ActorOp>()) {
      canonicalizeActor(actor);
    }
    return success();
  }
};

void IaraCanonicalizePass::runOnOperation() {
  pimpl = new Impl{this};
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}

} // namespace iara::passes::canonicalize
