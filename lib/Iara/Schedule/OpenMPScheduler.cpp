#include "Iara/Schedule/OpenMPScheduler.h"
#include "Iara/IaraOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/Support/LogicalResult.h"
#include <Util/MlirUtil.h>
#include <Util/RangeUtil.h>
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

namespace mlir::iara {
using namespace RangeUtil;
std::unique_ptr<OpenMPScheduler> OpenMPScheduler::create(ActorOp graph) {
  auto rv = std::make_unique<OpenMPScheduler>();
  rv->m_graph = graph;
  rv->m_module = graph->getParentOfType<ModuleOp>();
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

} // namespace mlir::iara