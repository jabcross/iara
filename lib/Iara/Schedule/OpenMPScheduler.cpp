#include "Iara/Schedule/OpenMPScheduler.h"
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
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SetVector.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LLVM.h>
#include <util/util.h>
#include <utility>

namespace mlir::iara {
std::unique_ptr<OpenMPScheduler> OpenMPScheduler::create(ActorOp graph) {
  auto rv = std::make_unique<OpenMPScheduler>();
  rv->m_graph = graph;
  return rv;
}

func::FuncOp createEmptyVoidFunctionWithBody(OpBuilder builder, StringRef name,
                                             Location loc) {
  auto rv =
      builder.create<func::FuncOp>(loc, name, builder.getFunctionType({}, {}));
  builder.atBlockEnd(rv.addEntryBlock()).create<func::ReturnOp>(loc);
  return rv;
}

LogicalResult OpenMPScheduler::emit(ModuleOp module) {
  OpBuilder builder{module};
  builder = builder.atBlockEnd(m_graph->getBlock());
  m_run_func = createEmptyVoidFunctionWithBody(builder, "__iara_run__",
                                               m_graph->getLoc());
  m_init_func = createEmptyVoidFunctionWithBody(builder, "__iara_init__",
                                                m_graph->getLoc());

  return success();
}

// template <class R, class F> auto operator|(R &&range, F &&transform) -> auto
// {
//   return llvm::map_range(std::forward<R>(range), std::forward<F>(transform));
// }

LogicalResult OpenMPScheduler::schedule() {

  // for (auto i : m_graph.getBody().getOps() /**/
  //                   | Map([](auto &op) {
  //                       op.dump();
  //                       return &op;
  //                     }) //
  //                   | Filter([](auto op) {})) {
  // }

  // auto x = foo(m_graph.getBody().getOps(), [](Operation &op) { return &op;
  // });

  // auto x = m_graph.getBody().getOps() | [](Operation *op) { return nullptr;
  // };

  bool has_cycle = false;
  auto sort_result = sortTopologically(m_graph->getBlock());
  llvm::outs() << std::format("Topological sort result: {}\n", sort_result);
  if (!checkSingleRate()) {
    m_graph->emitError() << "Graph is not single rate";
    return failure();
  }

  // Encapsulate every node in a dependency block

  auto encapsulateInDependency = [&](Operation *op) {
    auto builder = OpBuilder{op};
    auto new_dep = builder.create<DependencyOp>(
        op->getLoc(), builder.getNoneType(), mlir::ValueRange{});
    auto block = &new_dep.getBodyRegion().emplaceBlock();

    op->moveBefore(block, block->end());
    return new_dep;
  };

  auto addDependency = [&](DependencyOp dep, Value value) {
    SetVector<Value> operands;
    for (auto i : dep.getOperands()) {
      operands.insert(i);
    }
    operands.insert(value);
    dep->setOperands(operands.getArrayRef());
  };

  m_graph->walk([&](NodeOp node) {
    auto new_dep = encapsulateInDependency(node);
    SetVector<Value> deps;
    for (auto i : node->getOperands()) {
      deps.insert(
          i.getDefiningOp()->getParentOfType<DependencyOp>().getResult());
    }
    new_dep->setOperands(deps.getArrayRef());
  });

  DenseMap<Value, memref::AllocOp> alloc_sources;

  // Add allocations to pure outputs
  m_graph.walk([&](NodeOp node) {
    node.dump();
    auto new_operands = node.getOperands() | Into<SmallVector<Value>>();
    for (auto output : node.getPureOuts()) {
      auto node_dep = node->getParentOfType<DependencyOp>();
      auto builder = OpBuilder{node_dep};
      auto input_type = output.getType().cast<TensorType>();
      auto memref_type =
          MemRefType::get(input_type.getShape(), input_type.getElementType());
      auto new_input =
          builder.create<memref::AllocOp>(node.getLoc(), memref_type);
      auto dep = encapsulateInDependency(new_input);
      addDependency(node_dep, dep.getResult());
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

  m_graph.walk([&](NodeOp node) {
    auto builder = OpBuilder{node};
    builder.setInsertionPointAfter(node);
    for (auto input : node.getIn()) {
      auto dealloc_op = builder.create<memref::DeallocOp>(node.getLoc(),
                                                          alloc_sources[input]);
      dealloc_ops[dealloc_op] = input;
      auto dealloc_dep = encapsulateInDependency(dealloc_op);
      addDependency(dealloc_dep,
                    node->getParentOfType<DependencyOp>().getResult());
    };
  });

  return success(!has_cycle);
}
} // namespace mlir::iara