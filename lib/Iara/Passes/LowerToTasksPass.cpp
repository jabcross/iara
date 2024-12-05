#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "Iara/Passes/Schedule/OpenMPScheduler.h"
#include "Iara/Passes/Schedule/TaskScheduler.h"
#include "Util/RangeUtil.h"
#include "Util/rational.h"
#include "llvm/Support/Casting.h"
#include <cstddef>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/GlobalISel/Utils.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Transforms/ViewOpGraph.h>
#include <numeric>
#include <ratio>

using namespace RangeUtil;
using util::Rational;

namespace mlir::iara::passes {
#define GEN_PASS_DEF_LOWERTOTASKSPASS
#include "Iara/IaraPasses.h.inc"

namespace {

class LowerToTasksPass : public impl::LowerToTasksPassBase<LowerToTasksPass> {
public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect, memref::MemRefDialect, mlir::func::FuncDialect,
                    arith::ArithDialect, func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect>();
  }
  using impl::LowerToTasksPassBase<LowerToTasksPass>::LowerToTasksPassBase;

  SmallVector<Operation *> getNeighbors(Operation *op) {
    SmallVector<Operation *> neighbors;
    if (auto node = dyn_cast<NodeOp>(op)) {
      auto inputs = node.getIn() | IntoVector();
      inputs.append(node.getInout() | IntoVector());
      for (auto input : inputs) {
        neighbors.push_back(input.getDefiningOp());
      }
      for (auto output : node.getOut()) {
        auto users = output.getUsers() | IntoVector();
        assert(users.size() == 1);
        neighbors.emplace_back(users.front());
      }
      return neighbors;
    }
    if (auto edge = dyn_cast<EdgeOp>(op)) {
      auto users = edge.getOut().getUsers() | IntoVector();
      assert(users.size() == 1);
      return {edge.getIn().getDefiningOp(), users.front()};
    }
    llvm_unreachable("Unsupported operation");
  }

  LogicalResult annotateIterationNumbers(ActorOp actor) {
    if (actor.isKernelDeclaration())
      return success();
    assert(actor.isFlat());
    DenseMap<Operation *, util::Rational> iteration_numbers;
    // start walk
    SmallVector<Operation *> stack;
    stack.push_back(*actor.getOps<NodeOp>().begin());
    iteration_numbers[stack.back()] = {1};
    stack.back()->setAttr("iteration_attr",
                          OpBuilder(actor).getStringAttr(llvm::formatv("1")));
    while (not stack.empty()) {
      auto this_op = stack.back();
      stack.pop_back();
      for (auto neighbor : getNeighbors(this_op)) {
        if (iteration_numbers.contains(neighbor)) {
          auto this_n = iteration_numbers[this_op];
          auto neighbor_n = iteration_numbers[neighbor];
          if (isa<NodeOp>(neighbor) and (this_n != neighbor_n)) {
            this_op->emitError(
                "Graph is not admissible (inconsistent iteration numbers ")
                << llvm::formatv("{0} and {1}", this_n, neighbor_n) << "\n";
            this_op->dump();
            neighbor->dump();
            return failure();
          } else {
            continue;
          }
        }
        if (isa<NodeOp>(this_op)) {
          iteration_numbers[neighbor] = iteration_numbers[this_op];
          neighbor->setAttr("iteration_number",
                            OpBuilder(neighbor).getStringAttr(llvm::formatv(
                                "{0}", iteration_numbers[neighbor])));
          stack.push_back(neighbor);
        } else if (auto edge = dyn_cast<EdgeOp>(this_op)) {
          Rational flow_ratio = edge.getFlowRatio();
          if (neighbor == edge.getIn().getDefiningOp()) {
            flow_ratio = (flow_ratio / iteration_numbers[this_op]).normalized();
          } else {
            flow_ratio = (flow_ratio * iteration_numbers[this_op]).normalized();
          }
          iteration_numbers[neighbor] = flow_ratio;

          neighbor->setAttr("iteration_number",
                            OpBuilder(neighbor).getStringAttr(
                                llvm::formatv("{0}", flow_ratio)));
          stack.push_back(neighbor);
        } else {
          llvm_unreachable("Unsupported operation");
        }
      }
    }

    SmallVector<int64_t> denominators;
    // Assert that every node was visited
    for (auto node : actor.getOps<NodeOp>()) {
      if (not iteration_numbers.contains(node)) {
        node.emitError("Node was not visited.");
        node->dump();
        signalPassFailure();
        return failure();
      } else {
        denominators.push_back(iteration_numbers[node].denom);
      }
    }

    // All iteration numbers must be integers. Multiply all by lcm of
    // denominators.

    int64_t mult = std::reduce(denominators.begin(), denominators.end(), 1,
                               [](auto a, auto b) { return std::lcm(a, b); });

    for (auto [a, b] : iteration_numbers) {
      if (auto node = dyn_cast<NodeOp>(a)) {
        auto iteration_number = mult * b.num;
        node->setAttr("iteration_number",
                      OpBuilder(node).getI64IntegerAttr(iteration_number));
      }
    }
    return success();
  }

  void runOnOperation() final {
    auto module = getOperation();
    TaskScheduler task_scheduler{};
    module->walk([&](ActorOp actor_op) {
      if (not actor_op.isFlat() and not actor_op.isKernelDeclaration()) {
        module.emitError("Actor ")
            << actor_op.getSymName() << " is not flattened.";
        signalPassFailure();
        return;
      }
    });
    module->walk([&](ActorOp actor_op) {
      if (annotateIterationNumbers(actor_op).failed()) {
        module.emitError(
            "Graph is not admissible (inconsistent iteration numbers).");
        signalPassFailure();
        return;
      };

      if (task_scheduler.convertToTasks(actor_op).failed()) {
        module.emitError("Failed to lower all actors to task form.");
        signalPassFailure();
        return;
      };
    });

    // OpenMP part (transplanted here temporarily)

    OpenMPScheduler omp_scheduler{};

    auto ops = llvm::map_to_vector(getOperation().getOps(),
                                   [](Operation &op) { return &op; });

    for (auto op : ops) {
      if (auto actor = dyn_cast<ActorOp>(op)) {
        if (!actor.isKernelDeclaration())
          continue;

        auto impl_name = actor.getSymName();
        OpBuilder builder{actor};
        auto decl = builder.create<func::FuncOp>(actor.getLoc(), TypeRange{},
                                                 ValueRange{},
                                                 ArrayRef<NamedAttribute>{});

        decl->setAttr("sym_name", builder.getStringAttr(impl_name));
        decl->setAttr("sym_visibility", builder.getStringAttr("private"));
        decl->setAttr("function_type",
                      TypeAttr::get(actor.getImplFunctionType()));
        decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
        actor->erase();
        continue;
      }
      if (auto dag = dyn_cast<DAGOp>(op)) {
        omp_scheduler.convertIntoOpenMP(dag);
        dag.erase();
      }
    }

    for (auto actor : module.getOps<ActorOp>() | IntoVector()) {
      actor.erase();
    }
  }
};

} // namespace
} // namespace mlir::iara::passes
