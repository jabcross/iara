#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Dialect/Broadcast.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/EnvOption.h"
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

EdgeOp expandImplicitEdge(Value val) {
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
  return new_edge;
}

void expandImplicitEdgesAndBroadcasts(ActorOp actor) {
  // Ownership strategy for read-only fan-outs (env-only knob; default ON,
  // requires data-triggered — see iara::util::borrowModeActive). When off the
  // fan-out expands to the copy-all-but-one path below.
  bool borrow_mode = iara::util::borrowModeActive();

  // Expand all fan-outs. A borrow transform rebuilds its consumer nodes (to add
  // a logic output feeding the join), which invalidates any snapshot of the op
  // list — so find one fan-out, transform it, and re-scan from scratch, until
  // none remain. Each transform resolves one multi-use data result into
  // single-use edges, so this terminates.
  // A value used as a node `params` or `out_sizes` operand is a compile-time
  // scalar, not a data edge — it may be shared by many nodes (e.g. a computed
  // size feeding several producers) and must NOT be turned into a data
  // broadcast. Only in/inout uses are data edges that fan out.
  auto isDataUse = [](OpOperand &use) -> bool {
    Operation *owner = use.getOwner();
    if (auto n = dyn_cast<NodeOp>(owner)) {
      unsigned i = use.getOperandNumber();
      unsigned np = n.getParams().size();
      unsigned ndata = np + n.getIn().size() + n.getInout().size();
      return i >= np && i < ndata; // in/inout only (not params, not out_sizes)
    }
    // Edges / out-ports are data consumers; arith (index_cast, addi, ...) that
    // compute param/size values are not, and may be shared freely.
    return isa<EdgeOp, OutPortOp>(owner);
  };
  auto findAndExpandOne = [&]() -> bool {
    for (Operation *op : actor.getOps() | Pointers() | IntoVector()) {
      for (auto result : op->getResults()) {
        // Logic (`none`) outputs never fan out through a memcpy broadcast; a
        // logic fan-out is expressed as several distinct logic edges, not a
        // data-copying broadcast node.
        if (isa<NoneType>(result.getType()))
          continue;
        unsigned data_uses = 0;
        for (auto &use : result.getUses())
          if (isDataUse(use))
            data_uses++;
        if (data_uses <= 1)
          continue;
        if (borrow_mode && iara::dialect::broadcast::usesAllReadOnly(result))
          iara::dialect::broadcast::insertBroadcastBorrow(result);
        else
          iara::dialect::broadcast::insertBroadcast(result, false);
        return true; // op list mutated; caller re-scans
      }
    }
    return false;
  };
  while (findAndExpandOne()) {
  }

  // Explicit `@iara_broadcast` nodes (e.g. SIFT's topology emits these). Borrow
  // conversion for these runs POST-FLATTEN (in VirtualFIFOSchedulerPass): here
  // their consumers may still be actor instances, and adding a logic output to
  // an instance breaks its signature match with the actor definition. So just
  // specialize to the copy impl now; the scheduler pass rewrites borrowable ones.
  for (auto node : actor.getOps<NodeOp>() | IntoVector()) {
    if (node.getImpl() == "iara_broadcast") {
      broadcast::specializeBroadcast(node, false);
    }
  }

  for (auto node : actor.getOps<NodeOp>() | IntoVector()) {
    if (node.getImpl().starts_with("iara_broadcast")) {
      broadcast::getOrCodegenBroadcastImpl(node);
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
  // Logic (control-only) edges are typed `none`. Leave them bare: `none` is not
  // a valid tensor element type, so it must not be wrapped in a tensor, and it
  // carries no data to reshape.
  if (isa<NoneType>(old_type)) {
    return old_type;
  }
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
        auto res = in_port->getResult(0);
        auto new_type = canonicalizeType(res.getType());
        SmallVector<Value> dyn_sizes(in_port.getDynSizes());
        if (new_type != res.getType() || !in_port.getDynSizes().empty()) {
          DEF_OP(Value,
                 new_val,
                 InPortOp,
                 OpBuilder(op),
                 op->getLoc(),
                 new_type,
                 dyn_sizes,
                 in_port.getInout());
          res.replaceAllUsesWith(new_val);
          in_port->erase();
        }
        continue;
      }
      if (auto out_port = dyn_cast<OutPortOp>(op)) {
        auto in = out_port.getValue();
        auto new_type = canonicalizeType(in.getType());
        SmallVector<Value> dyn_sizes(out_port.getDynSizes());
        if (new_type != in.getType() || !out_port.getDynSizes().empty()) {
          SmallVector<Value> operands(dyn_sizes.begin(), dyn_sizes.end());
          operands.push_back(out_port.getValue());
          DEF_OP(OutPortOp,
                 new_val,
                 OutPortOp,
                 OpBuilder(op),
                 op->getLoc(),
                 TypeRange{},
                 operands,
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
