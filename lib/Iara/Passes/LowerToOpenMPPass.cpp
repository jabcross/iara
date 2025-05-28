#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Range.h"
#include "llvm/Support/Casting.h"
#include <cstddef>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Bufferization/Transforms/OneShotAnalysis.h>
#include <mlir/Dialect/DLTI/DLTI.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Linalg/IR/Linalg.h>
#include <mlir/Dialect/Math/IR/Math.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/IRMapping.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <mlir/Support/LLVM.h>

using namespace iara::util::range;

namespace iara::passes {
#define GEN_PASS_DEF_LOWERTOOPENMPPASS
#include "Iara/Dialect/IaraPasses.h.inc"

namespace {

class LowerToOpenMPPass
    : public impl::LowerToOpenMPPassBase<LowerToOpenMPPass> {
public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IaraDialect, memref::MemRefDialect, mlir::func::FuncDialect,
                    arith::ArithDialect, func::FuncDialect, LLVM::LLVMDialect,
                    mlir::math::MathDialect, mlir::linalg::LinalgDialect,
                    mlir::omp::OpenMPDialect, mlir::tensor::TensorDialect,
                    mlir::DLTIDialect>();
  }
  using impl::LowerToOpenMPPassBase<LowerToOpenMPPass>::LowerToOpenMPPassBase;

  void runOnOperation() final {

    OpenMPScheduler omp_scheduler{};

    auto ops = llvm::map_to_vector(getOperation().getOps(),
                                   [](Operation &op) { return &op; });

    for (auto op : ops) {
      if (auto actor = dyn_cast<ActorOp>(op)) {
        assert(actor.isKernelDeclaration());

        auto impl_name = actor.getSymName();
        OpBuilder builder{actor};
        auto decl = builder.create<func::FuncOp>(actor.getLoc(), TypeRange{},
                                                 ValueRange{},
                                                 ArrayRef<NamedAttribute>{});

        decl->setAttr("sym_name", builder.getStringAttr(impl_name));
        decl->setAttr("function_type",
                      TypeAttr::get(actor.getImplFunctionType()));

        decl->setAttr("sym_visibility", builder.getStringAttr("private"));
        decl->setAttr("llvm.emit_c_interface", builder.getUnitAttr());
        actor->erase();
        continue;
      }
      if (auto dag = dyn_cast<DAGOp>(op)) {
        omp_scheduler.convertIntoOpenMP(dag);
        dag.erase();
      }
    }
  }
};
} // namespace
} // namespace iara::passes
