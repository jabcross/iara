#ifndef IARA_PASSES_LOWERTOTASKSPASS_H
#define IARA_PASSES_LOWERTOTASKSPASS_H

#include "Iara/IaraOps.h"
#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include <mlir/Analysis/Presburger/Matrix.h>
#include <mlir/Dialect/DLTI/DLTI.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/OpenMP/OpenMPDialect.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/Builders.h>
#include <mlir/Pass/Pass.h>

namespace mlir::iara::passes {

using namespace RangeUtil;
using namespace mlir::iara::mlir_util;
using util::Rational;
template <class T> using Vec = llvm::SmallVector<T>;
template <class T> using Pair = std::pair<T, T>;
using mlir::presburger::IntMatrix;
using mlir::presburger::MPInt;

struct LowerToTasksPass
    : public PassWrapper<LowerToTasksPass, OperationPass<::mlir::ModuleOp>> {
public:
  ::llvm::StringRef getArgument() const override { return "sdf-to-tasks"; }
  ::llvm::StringRef getDescription() const override {
    return "Converts SDF dataflow to task DAG, generating memory management "
           "code.";
  }
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("LowerToTasksPass");
  }
  ::llvm::StringRef getName() const override { return "LowerToTasksPass"; }
  // static bool classof(const ::mlir::Pass *pass) {
  //   return pass->getTypeID() == ::mlir::TypeID::get<LowerToTasksPass>();
  // }
  // std::unique_ptr<::mlir::Pass> clonePass() const override {
  //   return std::make_unique<LowerToTasksPass>(
  //       *static_cast<const LowerToTasksPass *>(this));
  // }
  // /// Return the dialect that must be loaded in the context before this pass.
  // void getDependentDialects(::mlir::DialectRegistry &registry) const override
  // {} MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerToTasksPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect, memref::MemRefDialect, mlir::func::FuncDialect,
                    arith::ArithDialect, func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect,
                    mlir::DLTIDialect>();
  }

  void generateHeaderFile(mlir::OpBuilder &func_builder, llvm::StringRef path,
                          llvm::SmallVector<NodeOp> &nodes,
                          llvm::SmallVector<EdgeOp> &edges);

  DenseMap<std::pair<Type, size_t>, func::FuncOp> broadcast_impls;

  StringAttr FORWARD, BACKWARD;

  enum class Direction { Forward, Backward };

  SmallVector<std::tuple<Direction, EdgeOp, NodeOp>> getNeighbors(NodeOp node);

  NodeOp getMatchingDealloc(NodeOp alloc_node);

  LogicalResult annotateTotalFirings(ActorOp actor);
  func::FuncOp codegenBroadcastImpl(Value value, int64_t size);
  LogicalResult expandToBroadcast(OpResult &value);
  LogicalResult expandImplicitEdges(ActorOp actor);
  LogicalResult annotateNodeIDs(ActorOp actor);
  int64_t calculateRemainingDelays(EdgeOp edge);
  int64_t getConsPortIndex(EdgeOp edge);
  int64_t getBufferSizeWithDelays(EdgeOp edge);
  int64_t getBufferSizeWithoutDelays(EdgeOp edge);
  LogicalResult allocateContiguousBuffer(EdgeOp edge);
  LogicalResult allocateContiguousBuffers(ActorOp actor);
  LogicalResult annotateEdgeInfo(ActorOp actor);
  LogicalResult checkNoReverseInoutEdges(ActorOp actor);
  func::FuncOp createTaskFunc(NodeOp node, StringRef name, OpBuilder builder);
  EdgeOp createEdgeAdaptor(Value produced, Type consumed);
  int64_t getIntAttrValue(Operation *op, StringRef name);
  void populateAllocEdgeAttrs(EdgeOp edge);
  void populateDeallocEdgeAttrs(EdgeOp edge);
  SmallVector<Value> createAllocations(ValueRange values);
  void annotateAllocations(SmallVector<Value> &vals);
  SmallVector<NodeOp> createDeallocations(ValueRange values);
  void annotateDeallocations(SmallVector<NodeOp> &dealloc_nodes);
  LogicalResult generateAllocAndFreeNodes(ActorOp actor);
  func::FuncOp getFuncDecl(func::CallOp call, bool use_llvm_pointers = false);
  void generateAllocFiring(NodeOp node, int64_t iteration_lb,
                           int64_t iteration_ub, OpBuilder builder);
  void generateInitialFirings(NodeOp node, OpBuilder builder);
  int64_t getTotalInputBytes(NodeOp node);
  std::string getTaskFuncName(NodeOp node, OpBuilder func_builder);
  std::string codegenFirstBuffer(EdgeOp edge);
  LogicalResult codegenRuntimeInit(OpBuilder builder);
  LogicalResult codegenRuntimeFunction(ActorOp actor);
  LogicalResult taskify(ActorOp actor);
  void runOnOperation() final override;
};

inline void registerLowerToTasksPass() {
  mlir::registerPass([]() -> std::unique_ptr<::mlir::Pass> {
    return std::make_unique<LowerToTasksPass>();
  });
}

} // namespace mlir::iara::passes

#endif
