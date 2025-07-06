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
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Dominance.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::passes::canonicalize {

using namespace iara::util::mlir;
using namespace iara::util::range;

void expandImplicitEdge(Value val) {
  auto prod_node = dyn_cast<NodeOp>(val.getDefiningOp());
  assert(prod_node);
  auto users = val.getUsers() | IntoVector();
  assert(users.size() == 1);
  auto cons_node = dyn_cast<NodeOp>(users[0]);
  assert(cons_node);

  auto builder = OpBuilder(cons_node);
  auto new_edge =
      CREATE(EdgeOp,
             builder,
             builder.getFusedLoc({prod_node.getLoc(), cons_node.getLoc()}),
             val.getType(),
             val);
  val.replaceAllUsesExcept(new_edge.getOut(), {new_edge});
}

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
        expandImplicitEdge(output);
      }
    }
  }
  return;
}

Type canonicalizeType(Type old_type) {
  if (!isa<RankedTensorType>(old_type)) {
    return RankedTensorType::get({1}, old_type);
  }
  return old_type;
}

struct IaraCanonicalizePass::Impl {
  IaraCanonicalizePass *pass;

  Impl(IaraCanonicalizePass *pass) : pass(pass) {}

  void canonicalizeTypes(ActorOp actor) {
    for (Operation *op : actor.getOps() | Pointers() | IntoVector()) {
      if (auto in_port = dyn_cast<InPortOp>(op)) {
        assert(in_port.getDynSizes().empty());
        auto res = in_port->getResult(0);
        auto new_type = canonicalizeType(res.getType());
        if (new_type != res.getType()) {
          DEF_OP(Value,
                 new_val,
                 InPortOp,
                 OpBuilder(op),
                 op->getLoc(),
                 new_type,
                 {},
                 in_port.getInout());
          res.replaceAllUsesWith(new_val);
          in_port->erase();
        }
        continue;
      }
      if (auto out_port = dyn_cast<OutPortOp>(op)) {
        assert(out_port.getDynSizes().empty());
        auto in = out_port.getValue();
        auto new_type = canonicalizeType(in.getType());
        if (new_type != in.getType()) {
          DEF_OP(OutPortOp,
                 new_val,
                 OutPortOp,
                 OpBuilder(op),
                 op->getLoc(),
                 {},
                 out_port.getValue(),
                 out_port->getAttrs());
          out_port->erase();
        }
        continue;
      }
      if (auto edge = dyn_cast<EdgeOp>(op)) {
        auto in_type = edge.getIn().getType();
        auto out_type = edge.getOut().getType();
        auto new_in = canonicalizeType(in_type);
        auto new_out = canonicalizeType(out_type);
        if (new_in != in_type || new_out != out_type) {
          DEF_OP(EdgeOp,
                 new_edge,
                 EdgeOp,
                 OpBuilder(edge),
                 edge.getLoc(),
                 new_out,
                 edge.getIn(),
                 edge->getAttrs());
          edge.getOut().replaceAllUsesWith(new_edge.getOut());
          edge.erase();
        }
        continue;
      }
      if (!llvm::isa<NodeOp>(op) and !llvm::isa<EdgeOp>(op)) {
        // constants and compiletime ops?
        continue;
      }
      if (auto node = dyn_cast<NodeOp>(op)) {
        Vec<Value> old_inputs;
        Vec<Value> old_outputs;

        Vec<Type> new_input_types;
        Vec<Type> new_output_types;

        i64 differences = 0;
        for (auto operand : node.getAllInputs()) {
          old_inputs.push_back(operand);
          new_input_types.push_back(
              canonicalizeType(old_inputs.back().getType()));
          differences +=
              (old_inputs.back().getType() != new_input_types.back());
        }

        for (auto result : node.getOut()) {
          old_outputs.push_back(result);
          new_output_types.push_back(
              canonicalizeType(old_outputs.back().getType()));
          differences +=
              (old_outputs.back().getType() != new_output_types.back());
        }

        if (differences > 0) {

          DEF_OP(NodeOp,
                 new_node,
                 NodeOp,
                 OpBuilder(node),
                 node.getLoc(),
                 new_output_types,
                 node->getOperands(),
                 node->getAttrs());
          for (auto [old, _new] : llvm::zip(node.getOut(), new_node.getOut())) {
            old.replaceAllUsesWith(_new);
          }
          node->erase();
        }

        continue;
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
