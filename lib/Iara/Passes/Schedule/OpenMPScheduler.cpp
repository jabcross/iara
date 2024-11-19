#include "Iara/Passes/Schedule/OpenMPScheduler.h"
#include "Iara/IaraOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/Support/LogicalResult.h"
#include <Util/MlirUtil.h>
#include <Util/RangeUtil.h>
#include <iterator>
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

func::FuncOp OpenMPScheduler::convertIntoOpenMP(ActorOp actor) {
  if (actor.isKernel()) {
    auto impl_name = actor.getSymName();
    OpBuilder builder{actor};
    auto decl = builder.create<func::FuncOp>(
        actor.getLoc(), TypeRange{}, ValueRange{}, ArrayRef<NamedAttribute>{});

    decl->setAttr("sym_name", builder.getStringAttr(impl_name));
    decl->setAttr("sym_visibility", builder.getStringAttr("private"));
    decl->setAttr("function_type", TypeAttr::get(actor.getImplFunctionType()));
    decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
    return decl;
  }

  OpBuilder builder{actor};
  builder = builder.atBlockEnd(actor->getBlock());
  auto func_op = createEmptyVoidFunctionWithBody(builder, actor.getSymName(),
                                                 actor->getLoc());

  func_op.getFunctionBody().takeBody(actor->getRegion(0));

  auto ops = func_op.getOps() | Pointers() | Into<SmallVector<Operation *>>();

  // If function is empty (save for terminator), add a ReturnOp and return.
  if (ops.size() == 1 and isa<DepOp>(ops.back())) {
    ops.back()->erase();
    ops.clear();
  }

  if (ops.size() == 0) {
    OpBuilder{func_op}
        .atBlockEnd(&func_op.getBody().getBlocks().front())
        .create<func::ReturnOp>(func_op.getLoc());
    return func_op;
  }

  auto main_block = builder.createBlock(&func_op.getBody().getBlocks().front());

  // Store allocation pointers in stack

  auto alloca_builder = OpBuilder{func_op};
  alloca_builder.setInsertionPointToStart(main_block);
  SmallVector<Value> zero_index = {
      alloca_builder.create<arith::ConstantIndexOp>(func_op.getLoc(), 0)};

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

  func_op->walk([&](memref::AllocOp alloc_op) { allocate_ptr(alloc_op); });

  // Convert blocks into tasks

  DenseMap<Block *, DenseSet<int64_t>> input_deps, output_deps;
  DenseMap<Block *, int64_t> indices;

  for (auto [idx, block_ref] :
       llvm::enumerate(func_op.getFunctionBody().getBlocks())) {
    indices[&block_ref] = idx + 1;
  }
  for (auto &block_ref : func_op.getFunctionBody().getBlocks()) {
    for (auto succ : block_ref.getSuccessors()) {
      input_deps[succ].insert(indices[&block_ref]);
      output_deps[&block_ref].insert(indices[succ]);
    }
  }

  auto parallel_op =
      builder.atBlockEnd(main_block).create<omp::ParallelOp>(func_op->getLoc());
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
    auto rv =
        const_builder.create<arith::ConstantIntOp>(func_op.getLoc(), val, 64);
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
        func_op.getLoc(), LLVM::LLVMPointerType::get(builder.getContext()),
        getConstant(val));
    last_ptr = rv;
    return rv;
  });

  auto task_builder = OpBuilder{builder};

  task_builder.setInsertionPointToStart(
      &single_op->getRegion(0).getBlocks().front());

  auto blocks = func_op.getBlocks() | Drop(1) |
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
  task_builder.atBlockEnd(main_block).create<func::ReturnOp>(func_op.getLoc());

  return func_op;
}

} // namespace mlir::iara