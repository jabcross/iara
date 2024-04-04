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
#include <format>
#include <initializer_list>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SetVector.h>
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

LogicalResult OpenMPScheduler::convertIntoOpenMP() {
  OpBuilder builder{m_module};
  builder = builder.atBlockEnd(m_graph->getBlock());
  m_run_func = createEmptyVoidFunctionWithBody(builder, "__iara_run__",
                                               m_graph->getLoc());
  m_init_func = createEmptyVoidFunctionWithBody(builder, "__iara_init__",
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

// template <class R, class F> auto operator|(R &&range, F &&transform) -> auto
// {
//   return llvm::map_range(std::forward<R>(range), std::forward<F>(transform));
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

  // Encapsulate every node in a dependency block

  m_graph->walk([&](NodeOp node) { encapsulateInDependency(node); });

  m_graph->walk([&](NodeOp node) { setDeps(node); });

  DenseMap<Value, memref::AllocOp> alloc_sources;

  // Add allocations to pure outputs
  m_graph.walk([&](NodeOp node) {
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
  });

  // Forward inouts
  m_graph.walk([&](NodeOp node) {
    for (auto [input, output] : node.getInoutPairs()) {
      alloc_sources[output] = alloc_sources[input];
    };
  });

  // Free inputs

  DenseMap<memref::DeallocOp, Value> dealloc_ops;

  for (auto node : m_graph.getOps<NodeOp>()) {
    auto ins = node.getIn();
    if (ins.empty())
      continue;
    auto builder = OpBuilder{node};
    auto new_block = builder.createBlock(node->getBlock()->getNextNode());
    for (auto input : ins) {
      auto dealloc_op = builder.atBlockEnd(new_block).create<memref::DeallocOp>(
          node.getLoc(), alloc_sources[input]);
      dealloc_ops[dealloc_op] = input;
    };
    builder.atBlockEnd(new_block).create<DepOp>(node.getLoc(), BlockRange{});
    addDeps(llvm::cast<DepOp>(node->getBlock()->getTerminator()), {new_block});
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