#include "Iara/Schedule/TaskScheduler.h"
#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include "mlir/IR/TypeRange.h"
#include "llvm/ADT/STLExtras.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/YAMLParser.h>
#include <mlir-c/IR.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/ValueRange.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Transforms/TopologicalSortUtils.h>

namespace mlir::iara {

using namespace RangeUtil;

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

  auto getTask(Operation *op) -> Task {
    if (isa<DepOp>(op->getBlock()->getTerminator())) {
      return op->getBlock();
    }
    auto builder = OpBuilder{op};
    auto new_block = builder.createBlock(op->getBlock());
    auto dep_op =
        builder.atBlockEnd(new_block).create<DepOp>(op->getLoc(), BlockRange{});
    op->moveBefore(dep_op);
    return new_block;
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

  void bufferize() {
    for (auto node : actor.getOps<NodeOp>()) {
      solveOutputOwnerships(node);
    }
  }

  Task createSuccessorTask(Task task) {
    auto builder = OpBuilder{task.block->getParentOp()};
    auto new_block = builder.createBlock(task.block->getNextNode());
    builder.atBlockEnd(new_block).create<DepOp>(
        task.block->getParentOp()->getLoc(), BlockRange{});
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
  void createDeallocNode(OpResult &out, Task task) {
    OpBuilder{out.getOwner()}
        .atBlockTerminator(task.block)
        .create<NodeOp>(out.getOwner()->getLoc(), TypeRange{},
                        "__iara__dealloc__", ValueRange{}, ValueRange{out},
                        ValueRange{});
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
    // free dangling inouts
    for (auto [in, out] : node.getInoutPairs()) {
      if (out.getUses().empty()) {
        auto new_block = createSuccessorTask(getTask(out.getOwner()));
        createDeallocNode(out, new_block);
      }
    }
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
        if (!in_tasks.empty()) {
          for (auto task : in_tasks) {
            task.addDependent(getTask(inout_uses[0]->getOwner()));
          }
        }
      } else if (in_uses.size() > 0) {
        // Create a dealloc node for all `in` uses.
        auto last = findLastTask(in_tasks);
        auto dealloc_task = createSuccessorTask(last);
        createDeallocNode(out, last);
        for (auto task : in_tasks) {
          task.addDependent(dealloc_task);
        }
      }
      for (auto use : in_uses) {
        ownership_handled.insert(use->get());
      }
    }
  }

  // Assumes all ins have been allocated
  void bufferize(NodeOp node) {
    auto builder = OpBuilder{node};

    for (auto [in, out] : node.getInoutPairs()) {
      alloc_ops[out] = alloc_ops[in];
    }

    auto ins_to_free =
        node.getIn() |
        Filter([&](Value in) { return !ownership_handled.contains(in); }) |
        Into<SmallVector<Value>>();
    if (!ins_to_free.empty()) {
      auto frees = createSuccessorTask(getTask(node));
      for (auto in : ins_to_free) {
        if (dealloc_ops.find(in) != dealloc_ops.end()) {
          continue;
        }
        dealloc_ops[in] = //
            CREATE(memref::DeallocOp,
                   builder.atBlockTerminator(frees.block), //
                   node.getLoc(), alloc_ops[in]);
      }
    }
    if (!node.getPureOuts().empty()) {
      auto allocs = createPredecessorTask(getTask(node));
      for (auto out : node.getPureOuts()) {
        alloc_ops[out] =
            CREATE(memref::AllocOp, builder.atBlockTerminator(allocs.block),
                   node.getLoc(), cast<MemRefType>(out.getType()));
      }
    }
  }
};

LogicalResult TaskScheduler::convertToTasks() {

  TaskSchedule scheduler(m_graph);
  for (auto node : m_graph.getOps<NodeOp>()) {
    scheduler.getTask(node);
  }

  return success();
}

LogicalResult TaskScheduler::convertIntoSequential() {
  OpBuilder builder{m_module};
  builder.setInsertionPointToEnd(m_graph->getBlock());
  m_run_func = createEmptyVoidFunctionWithBody(builder, "__iara_run__",
                                               m_graph->getLoc());
  m_run_func.getFunctionBody().takeBody(m_graph->getRegion(0));
  m_graph.erase();
  auto new_block = builder.createBlock(&m_run_func.getBody().front());
  builder.atBlockEnd(new_block).create<func::ReturnOp>(m_graph->getLoc());
  for (auto op :
       m_run_func.getOps() | Pointers{} | Into<SmallVector<Operation *>>()) {
    if (llvm::isa<DepOp>(op))
      continue;
    op->moveBefore(&new_block->back());
  }
  for (auto block : m_run_func.getBody().getBlocks() | Pointers{} | Drop(1) |
                        Into<SmallVector<Block *>>()) {
    block->erase();
  }
  for (auto actor : m_module.getOps<ActorOp>() | Into<SmallVector<ActorOp>>()) {
    actor.erase();
  }
  return success();
}

} // namespace mlir::iara