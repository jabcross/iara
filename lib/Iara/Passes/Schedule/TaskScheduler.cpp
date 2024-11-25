#include "Iara/Passes/Schedule/TaskScheduler.h"
#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include "mlir/IR/TypeRange.h"
#include "llvm/ADT/STLExtras.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/YAMLParser.h>
#include <mlir-c/IR.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Transforms/TopologicalSortUtils.h>
#include <mlir/Transforms/ViewOpGraph.h>

namespace mlir::iara {

using namespace RangeUtil;

bool TaskScheduler::checkSingleRate(ActorOp actor) {
  if (!(actor.getParameterTypes().empty() && actor->getOperands().empty() &&
        actor.isFlat())) {
    return false;
  }

  // check that all nodes have a single rate

  if (!(llvm::all_of(actor.getOps<NodeOp>(),
                     [](NodeOp node) { return node.getParams().empty(); }))) {
    return false;
  }

  if (not llvm::all_of(actor.getOps<EdgeOp>(), [](EdgeOp edge_op) {
        return edge_op.getIn().getType() == edge_op.getOut().getType();
      })) {
    return false;
  };

  // todo: check all port connections
  return true;
}

struct Task;
struct Task {
  Block *block;
  Block *operator*() { return block; }
  Task(Block *block) : block(block) {}
  bool operator==(Task rhs) { return block == rhs.block; }
  void addDependent(Task dependent) {
    DepOp dep = cast<DepOp>(block->getTerminator());
    if (llvm::find(dep.getSuccessors(), dependent.block) !=
        dep.getSuccessors().end()) {
      return;
    }
    auto new_successors = dep.getSuccessors() | Into<SmallVector<Block *>>();
    new_successors.push_back(dependent.block);
    OpBuilder{dep}.create<DepOp>(dep->getLoc(), new_successors);
    dep->erase();
  }
  bool dependsOn(Task ancestor) {
    auto self = *this;
    if (self == ancestor) {
      llvm_unreachable("Undefined semantics");
      exit(1);
    }
    // graph search on the successors of ancestor, looking for block
    SmallVector<Task> stack{ancestor};
    DenseSet<Block *> visited;
    while (!stack.empty()) {
      auto current = stack.pop_back_val();
      for (auto successor : (*current)->getSuccessors()) {
        if (successor == *this) {
          return true;
        }
        if (!visited.contains(successor)) {
          visited.insert(successor);
          stack.push_back(Task(successor));
        }
      }
    }
    return false;
  };

  template <class R> bool isAncestorOfAny(R &&tasks) {
    if (llvm::find(tasks, *this) != tasks.end()) {
      llvm_unreachable("Undefined semantics");
      exit(1);
    }
    DenseSet<Block *> visited;
    SmallVector<Task> stack{*this};
    while (!stack.empty()) {
      auto current = stack.pop_back_val();
      for (auto successor : (*current)->getSuccessors()) {
        if (llvm::find(tasks, successor) != tasks.end()) {
          return true;
        }
        if (!visited.contains(successor)) {
          visited.insert(successor);
          stack.push_back(Task{successor});
        }
      }
    }
    return false;
  }
};

struct TaskSchedule {
  ActorOp actor;
  DenseMap<Value, memref::AllocOp> alloc_ops;
  DenseMap<Value, memref::DeallocOp> dealloc_ops;
  DenseSet<Value> ownership_handled; // if value is here, don't generate a free

  ActorOp operator*() { return actor; }
  TaskSchedule(ActorOp actor) : actor(actor) {}

  Task wrapIntoTask(Operation *op) {
    auto op_block = op->getBlock();
    if (op != &op_block->front()) {
      auto before = op->getBlock();
      op_block = before->splitBlock(op);
      OpBuilder(op).atBlockEnd(before).create<DepOp>(op->getLoc(),
                                                     BlockRange{});
    }
    if (op->getNextNode() != &op_block->back()) {
      auto next_op = op->getNextNode();
      op_block->splitBlock(next_op);
      OpBuilder(op).atBlockEnd(op_block).create<DepOp>(op->getLoc(),
                                                       BlockRange{});
    }
    return Task{op_block};
  }

  auto getTask(Operation *op) -> Task {
    assert(isa<DepOp>(op->getBlock()->getTerminator()));
    return op->getBlock();
  };

  auto setDeps(NodeOp op) {
    for (auto arg : op.getResults()) {
      for (auto user : arg.getUsers()) {
        getTask(op).addDependent(getTask(user));
      }
    }
  }

  static TaskSchedule convertToTaskForm(ActorOp actor) {
    TaskSchedule schedule{actor};
    for (auto node : actor.getOps<NodeOp>()) {
      schedule.setDeps(node);
    }
    return schedule;
  }

  Task createSuccessorTask(Task task) {
    auto builder = OpBuilder{task.block->getParentOp()};
    auto new_dep = CREATE(DepOp, builder.atBlockEnd(task.block),
                          task.block->getParentOp()->getLoc(), BlockRange{});
    auto new_block = task.block->splitBlock(new_dep);
    task.addDependent(Task{new_block});
    return new_block;
  }

  Task createPredecessorTask(Task task) {
    auto builder = OpBuilder{task.block->getParentOp()};
    auto new_block = builder.createBlock(task.block);
    builder.atBlockEnd(new_block).create<DepOp>(
        task.block->getParentOp()->getLoc(), BlockRange{});
    Task{new_block}.addDependent(task);
    return new_block;
  }

  Task createTaskBetween(Task source, Task sink) {
    auto rv = createSuccessorTask(source);
    rv.addDependent(sink);
    return rv;
  }

  Task findLastTask(SmallVector<Task> &tasks) {
    Task rv{tasks.front()};
    for (auto &block : rv.block->getParent()->getBlocks()) {
      if (llvm::find(tasks, Task{&block}) != tasks.end()) {
        rv = Task{&block};
      }
    }
    return rv;
  }

  // Creates a dealloc node op inside given task.
  void createDeallocNodes(SmallVector<Value> vals, Task task, Location loc) {
    for (auto val : vals) {
      CREATE(memref::DeallocOp,
             OpBuilder{val.getDefiningOp()}.atBlockTerminator(task.block), loc,
             val);
    }
  }

  // Inserts a copy node before this operand, effectively transforming an
  // inout operand into an in operand.
  // Return pointer to new use.
  OpOperand *createCopyTask(OpOperand &operand) {

    auto new_task = createTaskBetween(getTask(operand.get().getDefiningOp()),
                                      getTask(operand.getOwner()));
    auto copy_op =
        CREATE(NodeOp, //
               OpBuilder{operand.getOwner()}.atBlockTerminator(new_task.block),
               operand.getOwner()->getLoc(), //
               {operand.get().getType()},    //
               {"__iara_copy__"},            //
               {},                           //
               {operand.get()},              //
               {});

    operand.set(copy_op->getResult(0));
    return copy_op->getOpOperands().begin();
  }

  void solveOutputOwnerships(NodeOp node) {
    // for (auto [in, out] : node.getInoutPairs()) {
    //   if (out.getUses().empty()) {
    //     auto new_block = createSuccessorTask(getTask(out.getOwner()));
    //     createDeallocNode(out, new_block);
    //   }
    // }
    for (auto out : node.getOut()) {
      auto uses = out.getUses() | Pointers() | Into<SmallVector<OpOperand *>>();

      if (uses.empty()) {
        continue;
      }

      // sort uses into in and inout (inouts take ownership)

      SmallVector<OpOperand *> in_uses, inout_uses;
      for (auto use : uses) {
        auto user = cast<NodeOp>(use->getOwner());
        if (llvm::find(user.getIn(), use->get()) != user.getIn().end()) {
          in_uses.push_back(use);
        } else {
          inout_uses.push_back(use);
        }
      }

      // create copies for all inout users that are ancestors of an in user

      auto createCopyAndMoveToIn = [&](OpOperand *use) {
        auto new_use = createCopyTask(*use);
        in_uses.push_back(new_use);
        inout_uses.erase(llvm::find(inout_uses, use));
      };

      for (auto inout_use : inout_uses | Copy()) {
        if (getTask(inout_use->getOwner())
                .isAncestorOfAny(in_uses | Map([&](OpOperand *use) {
                                   return getTask(use->getOwner());
                                 }))) {
          createCopyAndMoveToIn(inout_use);
        }
      }

      // We can only give the buffer ownership to one inout. If there are
      // more, copy the rest.

      while (inout_uses.size() > 1) {
        createCopyAndMoveToIn(inout_uses.back());
      }

      assert(inout_uses.size() <= 1);

      // If there is a single inout user, it has the buffer ownership
      // but must be scheduled after every in use.
      auto in_tasks =
          in_uses |
          Map([&](OpOperand *use) { return getTask(use->getOwner()); }) |
          Into<SmallVector<Task>>();
      if (in_uses.size() > 0 and inout_uses.size() == 1) {
        // All in uses should be scheduled before the single inout use.
        if (!in_tasks.empty()) {
          auto inout_task = getTask(inout_uses[0]->getOwner());
          for (auto task : in_tasks) {
            task.addDependent(inout_task);
          }
          auto last = findLastTask(in_tasks);
          moveBlockAfter(inout_task.block, last.block);
        }
      } else if (in_uses.size() > 0) {
        // Create a dealloc node for all `in` uses.
        auto last = findLastTask(in_tasks);
        auto dealloc_task = createSuccessorTask(last);
        auto locs =
            in_uses |
            Map([&](OpOperand *use) { return use->getOwner()->getLoc(); }) |
            Into<SmallVector<Location>>();
        Location fused_loc = FusedLoc::get(node->getContext(), locs);
        createDeallocNodes({out}, dealloc_task, fused_loc);
        for (auto task : in_tasks) {
          task.addDependent(dealloc_task);
        }
      }
      for (auto use : in_uses) {
        ownership_handled.insert(use->get());
      }
    }
  }

  // Add dealloc symbolic nodes to consumed ins and dangling inouts.
  // Skips deallocs for values with shared ownership.
  void addDeallocs(NodeOp node) {

    SmallVector<Value> values_to_deallocate;

    // Pure ins need to be deallocated after this node runs.
    for (auto in : node.getIn()) {
      // Don't generate a dealloc if this is value has shared ownership.
      if (ownership_handled.contains(in))
        continue;
      values_to_deallocate.push_back(in);
    }

    // Outputs that are not consumed as well.
    for (auto out : node.getOut()) {
      if (out.getUses().empty()) {
        values_to_deallocate.push_back(out);
      }
    }

    if (values_to_deallocate.empty())
      return;

    auto new_block = createSuccessorTask(getTask(node));
    createDeallocNodes(values_to_deallocate, new_block, node.getLoc());
  }

  MemRefType memrefTypeFromTensorType(RankedTensorType tensortype) {
    return MemRefType::get(tensortype.getShape(), tensortype.getElementType());
  }

  // If the use is an inout port, propagate through the corresponding output
  // port.
  void propagateInout(OpOperand &use) {
    auto user = dyn_cast<NodeOp>(use.getOwner());
    if (!user)
      return;
    for (auto [in, out] : user.getInoutPairs()) {
      if (in == use.get()) {
        alloc_ops[out] = alloc_ops[in];
        for (auto &next_use :
             out.getUses() | Pointers() | Into<SmallVector<OpOperand *>>()) {
          next_use->set(in);
          propagateInout(*next_use);
        }
      }
    }
  }

  // Should only be called on a pure output, as an inout output will write on
  // preexisting memory.
  void createAlloc(OpResult &pure_out, Task task) {
    auto builder = OpBuilder{pure_out.getOwner()}.atBlockTerminator(task.block);
    auto alloc_op =
        CREATE(memref::AllocOp, builder, pure_out.getOwner()->getLoc(),
               memrefTypeFromTensorType(
                   dyn_cast<RankedTensorType>(pure_out.getType())),
               IntegerAttr());
    alloc_ops[pure_out] = alloc_op;
    appendOperand(pure_out.getOwner(), alloc_op);
    auto uses =
        pure_out.getUses() | Pointers() | Into<SmallVector<OpOperand *>>();
    for (auto &use : uses) {
      use->set(alloc_op);
      propagateInout(*use);
    }
  }

  // Adds allocations, convert copy nodes into memcpys and dealloc nodes into
  // dealloc ops.
  void bufferize() {

    auto getAllocTaskForBlock = memoize<Block *>([&](Block *block) -> Task {
      return createPredecessorTask(Task{block});
    });

    auto nodes = actor.getOps<NodeOp>() | Into<SmallVector<NodeOp>>();
    for (auto node : nodes) {
      if (node.getPureOuts().empty())
        continue;
      auto task = getAllocTaskForBlock(node->getBlock());
      for (auto out : node.getPureOuts()) {
        createAlloc(out, task);
      }
    }

    // Convert placeholder copies into CopyOps.
    for (auto node : actor.getOps<NodeOp>() | //
                         Filter([](NodeOp node) {
                           return node.getImpl() == "__iara_copy__";
                         }) |
                         Into<SmallVector<NodeOp>>()) {
      assert(node->getNumOperands() == 2);
      CREATE(memref::CopyOp, OpBuilder{node}, node->getLoc(),
             node->getOperand(0), node.getOperand(1));
      node->erase();
    }

    // Convert nodes into function calls
    for (auto node : actor.getOps<NodeOp>() | Into<SmallVector<NodeOp>>()) {
      auto call_op = CREATE(func::CallOp, OpBuilder{node}, node->getLoc(),
                            node.getImpl(), {}, node->getOperands());
      node.erase();
    }
  }

  void addMissingFunctionDeclarations() {
    auto module = actor->getParentOfType<ModuleOp>();
    auto builder = OpBuilder{module};
    builder.setInsertionPoint(this->actor);
    this->actor->walk([&](func::CallOp call_op) {
      if (!module.lookupSymbol(call_op.getCallee())) {
        auto func = CREATE(func::FuncOp, builder, call_op->getLoc(),
                           call_op.getCallee(),
                           builder.getFunctionType(call_op.getOperandTypes(),
                                                   call_op.getResultTypes()));
        func->setAttr("sym_visibility", builder.getStringAttr("private"));
        func->setAttr("emit_c_interface", builder.getUnitAttr());
      }
    });
  }
};
LogicalResult TaskScheduler::convertToTasks(ActorOp actor) {

  TaskSchedule scheduler(actor);
  auto nodes = actor.getOps<NodeOp>() | Into<SmallVector<NodeOp>>();
  for (auto node : nodes) {
    scheduler.wrapIntoTask(node);
  }
  for (auto node : nodes) {
    scheduler.setDeps(node);
  }
  for (auto node : nodes) {
    scheduler.solveOutputOwnerships(node);
  }
  for (auto node : nodes) {
    scheduler.addDeallocs(node);
  }
  scheduler.bufferize();

  scheduler.addMissingFunctionDeclarations();

  return success();
}

LogicalResult TaskScheduler::removeLoweredActors(ModuleOp module) {
  llvm::StringMap<ActorOp> actors;
  llvm::StringMap<func::FuncOp> funcs;

  for (auto actor : module.getOps<ActorOp>()) {
    actors[actor.getSymName()] = actor;
  }
  for (auto func : module.getOps<func::FuncOp>()) {
    funcs[func.getSymName()] = func;
  }

  for (auto &[name, actor] : actors) {
    if (funcs.contains(name)) {
      actor->erase();
    } else {
      return failure();
    }
  }
  return success();
}

func::FuncOp TaskScheduler::convertIntoSequential(ActorOp actor) {
  OpBuilder builder{actor};
  builder.setInsertionPointToEnd(actor->getBlock());
  auto func_op =
      createEmptyVoidFunctionWithBody(builder, "__iara_run__", actor->getLoc());
  func_op.getFunctionBody().takeBody(actor->getRegion(0));
  actor.erase();
  auto new_block = builder.createBlock(&func_op.getBody().front());
  builder.atBlockEnd(new_block).create<func::ReturnOp>(actor->getLoc());
  for (auto op :
       func_op.getOps() | Pointers{} | Into<SmallVector<Operation *>>()) {
    if (llvm::isa<DepOp>(op))
      continue;
    op->moveBefore(&new_block->back());
  }
  for (auto block : func_op.getBody().getBlocks() | Pointers{} | Drop(1) |
                        Into<SmallVector<Block *>>()) {
    block->erase();
  }
  return func_op;
}

} // namespace mlir::iara
