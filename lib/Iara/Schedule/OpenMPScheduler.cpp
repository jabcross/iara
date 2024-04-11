#include "Iara/Schedule/OpenMPScheduler.h"
#include "Iara/IaraOps.h"
#include "Iara/Schedule/Schedule.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/ExecutionEngine/CRunnerUtils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/TopologicalSortUtils.h"
#include <deque>
#include <format>
#include <initializer_list>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/ControlFlow/IR/ControlFlowOps.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/OpenMP/OpenMPDialect.h>
#include <mlir/IR/BlockSupport.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/Region.h>
#include <mlir/IR/Value.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Support/LLVM.h>
#include <util/util.h>
#include <utility>

namespace mlir::iara {
std::unique_ptr<OpenMPScheduler> OpenMPScheduler::create(ActorOp graph) {
  auto rv = std::make_unique<OpenMPScheduler>();
  rv->m_graph = graph;
  rv->m_module = graph->getParentOfType<ModuleOp>();
  return rv;
}

func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc) {
  auto rv =
      builder.create<func::FuncOp>(loc, name, builder.getFunctionType({}, {}));
  builder.atBlockEnd(rv.addEntryBlock()).create<func::ReturnOp>(loc);
  return rv;
}

bool OpenMPScheduler::checkSingleRate() {
  assert(m_graph.getParameterTypes().empty() &&
         m_graph->getOperands().empty() && m_graph.isFlat());

  // check that all nodes have a single rate

  assert(llvm::all_of(m_graph.getOps<NodeOp>(),
                      [](NodeOp node) { return node.getParams().empty(); }));

  // todo: check all port connections
  return true;
}

LogicalResult OpenMPScheduler::convertIntoSequential() {
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

LogicalResult OpenMPScheduler::convertIntoOpenMP() {
  OpBuilder builder{m_module};
  builder = builder.atBlockEnd(m_graph->getBlock());
  m_run_func = createEmptyVoidFunctionWithBody(builder, "__iara_run__",
                                               m_graph->getLoc());

  m_run_func.getFunctionBody().takeBody(m_graph->getRegion(0));

  auto actors = m_module.getOps<ActorOp>() | Into<SmallVector<ActorOp>>();

  for (auto actor : actors) {
    actor.erase();
  }

  if (m_run_func.getOps().empty()) {
    // No work to do; add return op and exit

    OpBuilder{m_run_func}
        .atBlockEnd(&m_run_func.getBody().getBlocks().front())
        .create<func::ReturnOp>(m_run_func.getLoc());
    return success();
  }

  auto main_block =
      builder.createBlock(&m_run_func.getBody().getBlocks().front());

  // Store allocation pointers in stack

  auto alloca_builder = OpBuilder{m_run_func};
  alloca_builder.setInsertionPointToStart(main_block);
  SmallVector<Value> zero_index = {
      alloca_builder.create<arith::ConstantIndexOp>(m_run_func.getLoc(), 0)};

  auto allocate_ptr = [&](memref::AllocOp alloc_op) {
    auto alloca_op = alloca_builder.create<memref::AllocaOp>(
        alloc_op.getLoc(), MemRefType::get({1}, alloc_op.getType()));
    auto users =
        alloc_op.getResult().getUsers() | Into<SmallVector<Operation *>>();
    for (auto user : users) {
      auto load_op = OpBuilder{user}.create<memref::LoadOp>(
          user->getLoc(), alloca_op, zero_index);
      user->replaceUsesOfWith(alloc_op, load_op);
    }
    auto store_builder = OpBuilder{alloc_op};
    store_builder.setInsertionPointAfter(alloc_op);
    store_builder.create<memref::StoreOp>(alloc_op.getLoc(), alloc_op,
                                          alloca_op, zero_index);
    return alloca_op;
  };

  m_run_func->walk([&](memref::AllocOp alloc_op) { allocate_ptr(alloc_op); });

  // Convert blocks into tasks

  DenseMap<Block *, DenseSet<int64_t>> input_deps, output_deps;
  DenseMap<Block *, int64_t> indices;

  for (auto [idx, block_ref] :
       llvm::enumerate(m_run_func.getFunctionBody().getBlocks())) {
    indices[&block_ref] = idx + 1;
  }
  for (auto &block_ref : m_run_func.getFunctionBody().getBlocks()) {
    for (auto succ : block_ref.getSuccessors()) {
      input_deps[succ].insert(indices[&block_ref]);
      output_deps[&block_ref].insert(indices[succ]);
    }
  }

  auto parallel_op = builder.atBlockEnd(main_block)
                         .create<omp::ParallelOp>(m_run_func->getLoc());
  auto single_op =
      builder.atBlockBegin(builder.createBlock(&parallel_op->getRegion(0)))
          .create<omp::SingleOp>(parallel_op.getLoc(), ValueRange{},
                                 ValueRange{});
  builder.atBlockEnd(&parallel_op->getRegion(0).getBlocks().front())
      .create<omp::TerminatorOp>(parallel_op.getLoc());

  builder.atBlockEnd(builder.createBlock(&single_op->getRegion(0)))
      .create<omp::TerminatorOp>(parallel_op.getLoc());

  auto in_attr = mlir::omp::ClauseTaskDependAttr::get(
      builder.getContext(), omp::ClauseTaskDepend::taskdependin);
  auto out_attr = mlir::omp::ClauseTaskDependAttr::get(
      builder.getContext(), omp::ClauseTaskDepend::taskdependout);

  builder.setInsertionPointToStart(
      &single_op->getRegion(0).getBlocks().front());

  auto const_builder = OpBuilder{parallel_op};
  Operation *last_const = nullptr;
  auto getConstant = memoize<int64_t>([&](int64_t val) {
    if (last_const)
      const_builder.setInsertionPointAfter(last_const);
    auto rv = const_builder.create<arith::ConstantIntOp>(m_run_func.getLoc(),
                                                         val, 64);
    last_const = rv;
    return rv;
  });

  auto pointer_builder = OpBuilder{parallel_op};
  Operation *last_ptr = nullptr;
  pointer_builder.setInsertionPointAfter(getConstant(1));

  auto getFakePointer = memoize<int64_t>([&](int64_t val) -> LLVM::IntToPtrOp {
    if (last_ptr)
      pointer_builder.setInsertionPointAfter(last_ptr);
    auto rv = pointer_builder.create<LLVM::IntToPtrOp>(
        m_run_func.getLoc(), LLVM::LLVMPointerType::get(builder.getContext()),
        getConstant(val));
    last_ptr = rv;
    return rv;
  });

  auto task_builder = OpBuilder{builder};

  task_builder.setInsertionPointToStart(
      &single_op->getRegion(0).getBlocks().front());

  auto blocks = m_run_func.getBlocks() | Drop(1) |
                Map([](Block &block) { return &block; }) |
                Into<SmallVector<Block *>>();

  for (auto block : blocks) {
    auto loc = block->front().getLoc();
    auto dep_attrs = SmallVector<Attribute>{};
    auto dep_vars = SmallVector<Value>{};

    for (auto in_dep : input_deps[block]) {
      dep_attrs.push_back(in_attr);
      dep_vars.push_back(getFakePointer(in_dep));
    }
    dep_attrs.push_back(out_attr);
    dep_vars.push_back(getFakePointer(indices[block]));

    auto task_op = task_builder.create<omp::TaskOp>(
        loc, Value{}, Value{}, UnitAttr{}, UnitAttr{}, ValueRange{},
        ArrayAttr{}, Value{}, task_builder.getArrayAttr(dep_attrs), dep_vars,
        ValueRange{}, ValueRange{});
    auto tmp_block = builder.createBlock(&task_op.getRegion());
    block->moveBefore(tmp_block);
    tmp_block->erase();
    block->eraseArguments(0, block->getNumArguments());
    block->back().erase();
    task_builder.atBlockEnd(block).create<omp::TerminatorOp>(task_op.getLoc());
  }
  task_builder.atBlockEnd(main_block)
      .create<func::ReturnOp>(m_run_func.getLoc());

  return success();
}

// template <class R, class F> auto operator|(R &&range, F &&transform) ->
// auto
// {
//   return llvm::map_range(std::forward<R>(range),
//   std::forward<F>(transform));
// }

LogicalResult OpenMPScheduler::convertToTasks() {

  // check if region or block is empty

  if (m_graph.getBody().empty() or m_graph.getOps<NodeOp>().empty()) {
    m_graph.emitRemark() << "Graph is empty";
    return success();
  }

  bool has_cycle = false;
  auto sort_result = sortTopologically(m_graph->getBlock());
  llvm::errs() << std::format("Topological sort result: {}\n", sort_result);
  if (!checkSingleRate()) {
    m_graph->emitError() << "Graph is not single rate";
    return failure();
  }

  // Helper functions

  auto encapsulateInDependency = [&](Operation *op) {
    auto builder = OpBuilder{op};
    auto new_block = builder.createBlock(op->getBlock());
    auto dep_op =
        builder.atBlockEnd(new_block).create<DepOp>(op->getLoc(), BlockRange{});
    op->moveBefore(dep_op);
    return new_block;
  };

  auto addDeps = [](DepOp dep, std::initializer_list<Block *> blocks) {
    auto dep_succs = dep.getSuccessors();
    auto succs = dep_succs | Into<SmallVector<Block *>>();
    for (auto block : blocks)
      if (llvm::find(dep_succs, block) == dep_succs.end()) {
        succs.push_back(block);
      }
    OpBuilder{dep}.create<DepOp>(dep->getLoc(), succs);
    dep->erase();
  };

  auto setDeps = [&](Operation *op) {
    for (auto arg : op->getOperands()) {
      if (arg.getDefiningOp()->getBlock() == op->getBlock()) {
        continue;
      }
      addDeps(
          llvm::cast<DepOp>(arg.getDefiningOp()->getBlock()->getTerminator()),
          {op->getBlock()});
    }
  };

  // Returns true if descendant depends on ancestor.
  auto dependsOn = [&](Block *descendant, Block *ancestor) {
    if (descendant == ancestor) {
      llvm_unreachable("Undefined semantics");
      exit(1);
    }
    // graph search on the successors of other, looking for block
    SmallVector<Block *> stack{ancestor};
    DenseSet<Block *> visited;
    while (!stack.empty()) {
      auto current = stack.pop_back_val();
      for (auto successor : current->getSuccessors()) {
        if (successor == descendant) {
          return true;
        }
        if (!visited.contains(successor)) {
          visited.insert(successor);
          stack.push_back(successor);
        }
      }
    }
    return false;
  };

  auto topoSortTasks = [&]() -> LogicalResult {
    // find blocks with no predecessors
    SmallVector<Block *> final_order;
    std::deque<Block *> queue;
    DenseMap<Block *, int> block_number;
    int counter = 1;
    for (auto &block : m_graph.getBody().getBlocks()) {
      if (block.getPredecessors().empty()) {
        queue.push_back(&block);
        block_number[&block] = counter++;
      }
    }
    while (queue.size() > 0) {
      auto current = queue.front();
      queue.pop_front();
      final_order.push_back(current);
      for (auto successor : current->getSuccessors()) {
        auto it = block_number.find(successor);
        if (it != block_number.end()) {
          if (it->second <= block_number[current]) {
            return failure();
          }
        } else {
          block_number[successor] = counter++;
          queue.push_back(successor);
        }
      }
    }

    for (auto block : llvm::reverse(final_order)) {
      block->moveBefore(&m_graph.getBody().front());
    }
    return success();
  };

  auto getLastBlock = [](SmallVector<Block *> &blocks) {
    assert(blocks.size() > 0);
    Block *rv = nullptr;
    for (auto &block : blocks.front()->getParent()->getBlocks()) {
      if (llvm::find(blocks, &block) != blocks.end()) {
        rv = &block;
      }
    }
    assert(rv != nullptr);
    return rv;
  };

  DenseMap<Value, memref::AllocOp> alloc_sources;

  // Creates a task that deallocs the given allocs, and that deppends on the
  // given predecessors.
  auto createDealloc = [&](SmallVector<memref::AllocOp> allocs,
                           SmallVector<Block *> preds) {
    Block *last_dep = getLastBlock(preds);

    auto builder = OpBuilder{last_dep->getParentOp()};
    assert(last_dep->getNextNode() != nullptr);
    auto new_block = builder.createBlock(last_dep->getNextNode());
    for (auto input : allocs) {
      auto dealloc_op = builder.atBlockEnd(new_block).create<memref::DeallocOp>(
          input.getLoc(), input);
    };
    builder.atBlockEnd(new_block).create<DepOp>(
        last_dep->getParentOp()->getLoc(), BlockRange{});
    for (auto dep : preds) {
      addDeps(llvm::cast<DepOp>(dep->getTerminator()), {new_block});
    }
  };

  // Encapsulate every node in a dependency block

  m_graph->walk([&](NodeOp node) { encapsulateInDependency(node); });

  m_graph->walk([&](NodeOp node) { setDeps(node); });

  // These reads will not generate a free.
  DenseSet<Value> weak_reads;

  // For each output port:

  for (auto node : m_graph.getOps<NodeOp>()) {
    // Add allocations to pure outputs
    auto new_operands = node.getOperands() | Into<SmallVector<Value>>();
    for (auto output : node.getPureOuts()) {
      auto builder = OpBuilder{node};
      auto input_type = output.getType().cast<TensorType>();
      auto memref_type =
          MemRefType::get(input_type.getShape(), input_type.getElementType());
      auto new_input =
          builder.create<memref::AllocOp>(node.getLoc(), memref_type);
      auto new_block = encapsulateInDependency(new_input);
      addDeps(llvm::cast<DepOp>(new_block->getTerminator()),
              {node->getBlock()});
      alloc_sources[output] = new_input;
      new_operands.push_back(new_input);
    }
    node->setOperands(new_operands);

    DenseSet<OpResult> inout_outs;

    // Forward inouts
    for (auto [in, out] : node.getInoutPairs()) {
      inout_outs.insert(out);
      alloc_sources[out] = alloc_sources[in];
    }

    // Determine copies for implicit broadcasts
    for (auto output : node.getResults()) {
      SmallVector<OpOperand *> uses_taking_ownership{};
      SmallVector<OpOperand *> read_only_uses{};
      for (auto &use : output.getUses()) {
        auto user = llvm::cast<NodeOp>(use.getOwner());
        if (user.isInoutInput(use)) {
          uses_taking_ownership.push_back(&use);
        } else {
          read_only_uses.push_back(&use);
        }
      }
      while (uses_taking_ownership.size() > 1) {
        llvm_unreachable("unimplemented: more than one output takes ownership");
        exit(1);
      }
      if (uses_taking_ownership.size() == 1) {
        auto move_use = uses_taking_ownership.front();
        // Try to schedule this execution after all of the other reads to
        // avoid a copy.
        bool possible = llvm::none_of(read_only_uses, [&](OpOperand *readonly) {
          return !dependsOn(readonly->getOwner()->getBlock(),
                            move_use->getOwner()->getBlock());
        });

        if (possible) {
          for (auto &readonly : read_only_uses) {
            weak_reads.insert(readonly->get());
            addDeps(
                cast<DepOp>(readonly->getOwner()->getBlock()->getTerminator()),
                {move_use->getOwner()->getBlock()});
          }
          if (topoSortTasks().failed()) {
            llvm_unreachable(
                "unimplemented: topological sort failed when elliding copy");
            exit(1);
          }
        }
      } else {
        // Create the free here.
        for (auto &readonly : read_only_uses) {
          weak_reads.insert(readonly->get());
        }
        auto alloc = alloc_sources[output];
        SmallVector<Block *> deps;
        for (auto &readonly : read_only_uses) {
          deps.push_back(readonly->getOwner()->getBlock());
        }
        createDealloc({alloc}, deps);
      }
    }
  }

  // Free remaining inputs

  for (auto node : m_graph.getOps<NodeOp>()) {
    auto allocs =
        node.getIn() |
        Filter([&](Value value) { return !weak_reads.contains(value); }) |
        Map([&](Value value) { return alloc_sources[value]; }) |
        Into<SmallVector<memref::AllocOp>>();
    if (allocs.empty())
      continue;
    createDealloc(allocs, {node->getBlock()});
  }

  // Generate function declarations

  // Change node ops to call ops

  auto getImplFunc = [&](NodeOp node) -> func::FuncOp {
    auto impl_name = llvm::formatv("{0}_impl", node.getImpl()).str();
    if (auto impl =
            m_module.getOps<func::FuncOp>() | Find([&](func::FuncOp func) {
              return func.getName() == impl_name;
            })) {
      return impl;
    }
    auto kernel =
        m_module.getOps<ActorOp>() | Find([&](ActorOp actor) {
          return actor.getName() == node.getImpl() && actor.isKernel();
        });
    if (kernel) {
      OpBuilder builder{kernel};
      auto decl = builder.create<func::FuncOp>(kernel.getLoc(), TypeRange{},
                                               ValueRange{},
                                               ArrayRef<NamedAttribute>{});

      decl->setAttr("sym_name", builder.getStringAttr(impl_name));
      decl->setAttr("sym_visibility", builder.getStringAttr("private"));
      decl->setAttr("function_type",
                    TypeAttr::get(kernel.getImplFunctionType()));
      decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());

      return decl;
    }
    node->emitError() << "Could not find node's implementation ("
                      << node.getImpl() << ")";
    return {};
  };

  // Convert tensor ops to memref ops, in reverse order

  auto nodes = m_graph.getOps<NodeOp>() | Into<SmallVector<NodeOp>>();

  for (auto node : llvm::reverse(nodes)) {
    auto builder = OpBuilder{node};
    auto new_operands = node.getOperands() | Into<SmallVector<Value>>();

    for (auto [idx, input] : llvm::enumerate(node.getIn())) {
      if (input.getType().isa<TensorType>()) {
        auto memref = alloc_sources[input];
        auto index = node.getParams().size() + idx;
        node.setOperand(index, memref.getResult());
      }
    }
    for (auto [idx, input] : llvm::enumerate(node.getInout())) {
      if (input.getType().isa<TensorType>()) {
        auto memref = alloc_sources[input];
        auto index = node.getParams().size() + node.getIn().size() + idx;
        node.setOperand(index, memref.getResult());
      }
    }
  }
  m_graph.walk([&](NodeOp node) {
    auto builder = OpBuilder{node};
    auto impl_func = getImplFunc(node);
    auto call_op =
        builder.create<func::CallOp>(node.getLoc(), impl_func, ValueRange{});
    call_op->setOperands(node.getOperands());
    assert(node->getUsers().empty());
    node->erase();
  });

  Block *last_block = &m_graph.getRegion().back();

  assert(last_block->getOperations().size() == 1);
  last_block->erase();

  return success(!has_cycle);
}
} // namespace mlir::iara