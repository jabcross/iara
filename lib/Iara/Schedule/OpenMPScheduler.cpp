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
#include <initializer_list>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/ControlFlow/IR/ControlFlowOps.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
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

  return success();
}

// template <class R, class F> auto operator|(R &&range, F &&transform) -> auto
// {
//   return llvm::map_range(std::forward<R>(range), std::forward<F>(transform));
// }

LogicalResult OpenMPScheduler::convertToTasks() {

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
    node.dump();
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