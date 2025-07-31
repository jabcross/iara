#include "Iara/Dialect/IaraOps.h"
// #include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/BreakLoops.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Passes/VirtualFIFO/VirtualFIFOSchedulerPass.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Chunk.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <cstddef>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/LogicalResult.h>
#include <mlir/Conversion/ConvertToLLVM/ToLLVMPass.h>
#include <mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h>
#include <mlir/Conversion/LLVMCommon/StructBuilder.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/Dialect/LLVMIR/Transforms/Passes.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/DialectImplementation.h>
#include <mlir/IR/ExtensibleDialect.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Support/LogicalResult.h>

using namespace iara::util::range;
using namespace iara::passes::virtualfifo::sdf;
using namespace iara::util::mlir;

namespace iara::passes::virtualfifo {

using namespace func;
using namespace iara::passes::common::codegen;
using namespace iara::passes::virtualfifo::codegen;

struct VirtualFIFOSchedulerPass::Impl {
  VirtualFIFOSchedulerPass *pass;
  DenseMap<EdgeOp, Value> runtime_fifo_pointers;
  DenseMap<EdgeOp, LLVM::GlobalOp> edge_info_global_ops;
  DenseMap<NodeOp, LLVM::GlobalOp> node_infos;
  DenseMap<EdgeOp, LLVM::GlobalOp> delay_values;

  Impl(VirtualFIFOSchedulerPass *pass) : pass(pass) {}

  MLIRContext *ctx() { return &pass->getContext(); }

  Type i64type() { return IntegerType::get(ctx(), 64); }

  LLVM::LLVMPointerType llvm_pointer_type() {
    return LLVM::LLVMPointerType::get(ctx());
  }
  LLVM::LLVMVoidType llvm_void_type() { return LLVM::LLVMVoidType::get(ctx()); }

  LLVM::LLVMStructType chunk_type() {
    return cast<LLVM::LLVMStructType>(VirtualFIFO_Chunk::getMLIRType(ctx()));
  }

  LLVM::LLVMFunctionType wrapper_type() {
    return LLVM::LLVMFunctionType::get(
        llvm_void_type(),
        {IntegerType::get(ctx(), 64), llvm_pointer_type()},
        false);
  }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  // The single argument is a pointer to a buffer of pointers to the memory
  // accessed by the kernel.
  LLVM::LLVMFuncOp getOrCodegenNodeWrapper(ModuleOp module,
                                           OpBuilder mod_builder,
                                           NodeCodegenData node_codegen_data) {

    auto node_op = node_codegen_data.node_op;
    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx());
    if (node_op.isAlloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_alloc")) {
        return existing;
      }
      auto rv = CREATE(LLVM::LLVMFuncOp,
                       mod_builder,
                       module.getLoc(),
                       "iara_runtime_alloc",
                       wrapper_type());
      rv.setVisibility(mlir::SymbolTable::Visibility::Private);
      rv->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
      return rv;
    }
    if (node_op.isDealloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_dealloc")) {
        return existing;
      }
      auto rv = CREATE(LLVM::LLVMFuncOp,
                       mod_builder,
                       module.getLoc(),
                       "iara_runtime_dealloc",
                       wrapper_type());
      rv.setVisibility(mlir::SymbolTable::Visibility::Private);
      rv->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
      return rv;
    }

    auto sym_name = llvm::formatv("iara_node_wrapper_{0}_{1}",
                                  node_codegen_data.static_info.id,
                                  node_op.getImpl())
                        .str();

    if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(sym_name)) {
      return existing;
    }

    auto num_buffers = node_op.getIn().size() + node_op.getOut().size();

    auto wrapper = CREATE(LLVM::LLVMFuncOp,
                          mod_builder,
                          node_op.getLoc(),
                          sym_name,
                          wrapper_type());

    wrapper.setVisibility(mlir::SymbolTable::Visibility::Public);
    wrapper->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());

    // addEntryBlock broken?

    auto *block = &wrapper.getFunctionBody().emplaceBlock();
    auto _wrapper_type = wrapper_type();
    for (auto type : _wrapper_type.getParams()) {
      block->addArgument(type, wrapper->getLoc());
    }

    //   auto block = wrapper.addEntryBlock();

    auto func_builder = OpBuilder::atBlockBegin(block);

    // kernel must have format void name(params*, in*, inout*, out*).

    SmallVector<Value> kernel_args;

    for (auto p : node_op.getParams()) {
      auto constant = dyn_cast<arith::ConstantOp>(p.getDefiningOp());
      assert(constant && "Params should have been uniqued by now.");
      auto new_const = func_builder.clone(*constant);
      kernel_args.push_back(new_const->getResult(0));
    }

    for (size_t i = 0; i < num_buffers; i++) {

      // arg1 is a pointer to the first chunk. we want the `data` field.

      // get pointer from array
      auto pointer_to_pointer = CREATE(LLVM::GEPOp,
                                       func_builder,
                                       node_op.getLoc(),
                                       opaque_ptr_type,
                                       chunk_type(),
                                       func_builder.getBlock()->getArgument(1),
                                       {(i32)i, 2 /* `data field` */});

      auto pointer_to_data = CREATE(LLVM::LoadOp,
                                    func_builder,
                                    node_op.getLoc(),
                                    opaque_ptr_type,
                                    pointer_to_pointer);

      kernel_args.push_back(pointer_to_data);
    }

    auto func = module.lookupSymbol(node_op.getImpl());

    if (!func) {
      auto kernel_call = CREATE(LLVM::CallOp,
                                func_builder,
                                node_op.getLoc(),
                                TypeRange{},
                                node_op.getImpl(),
                                kernel_args);
      ensureFuncDeclExists(kernel_call);
    } else if (auto llvm_func = dyn_cast<LLVM::LLVMFuncOp>(func)) {
      DEF_OP(auto,
             kernel_call,
             LLVM::CallOp,
             func_builder,
             node_op.getLoc(),
             TypeRange{},
             node_op.getImpl(),
             kernel_args);
    } else if (auto mlir_func = dyn_cast<func::FuncOp>(func)) {
      llvm_unreachable("No MLIR functions at this point,");
    }

    CREATE(LLVM::ReturnOp, func_builder, node_op.getLoc(), ValueRange{});

    return wrapper;
  }

  ActorOp getMainActor(ModuleOp module) {
    // At this point, there should be only one actor with a body.

    DenseSet<ActorOp> not_top_level;
    Vec<ActorOp> definitions;

    if (pass->main_actor.hasValue()) {
      auto actor = module.lookupSymbol<ActorOp>(pass->main_actor.getValue());
      if (!actor)
        llvm::errs() << "Provided actor name not found: "
                     << pass->main_actor.getValue();
      assert(actor && "Provided actor name not found");
      return actor;
    }

    for (auto actor : module.getOps<ActorOp>()) {
      if (actor.isKernelDeclaration()) {
        continue;
      }
      if (not_top_level.contains(actor)) {
        continue;
      }
      definitions.push_back(actor);
    }
    if (definitions.size() > 1) {
      module->emitError("At this point, the graph should have had only one "
                        "top level candidate; there "
                        "are ")
          << definitions.size() << " here.";
      llvm_unreachable("");
    }
    if (definitions.size() == 0) {
      module->emitError("No actor definitions found.");
      llvm_unreachable("");
    }
    return definitions[0];
  }

  void codegenEdgeInit(OpBuilder builder, EdgeOp edge, VirtualFIFO_Edge &info) {
  }

  void upgradeDelaysToDenseArrays(ActorOp actor) {
    for (auto edge : actor.getOps<EdgeOp>()) {
      if (!edge->hasAttr("delay")) {
        continue;
      }

      if (auto int_attr =
              llvm::dyn_cast_or_null<IntegerAttr>(edge["delay"].get())) {
        // fill with zeros
        auto elem_type = getElementTypeOrSelf(edge.getIn().getType());
        size_t num_elems = int_attr.getInt();
        size_t size_elem = getTypeSize(elem_type, DataLayout::closest(edge));
        char *zeros = (char *)calloc(num_elems, size_elem);
        auto delay_attr =
            DenseArrayAttr::get(elem_type,
                                int_attr.getInt(),
                                ArrayRef<char>(zeros, num_elems * size_elem));
        edge->setAttr("delay", delay_attr);
        free(zeros);
      }
    }
  }

  LogicalResult codegenStaticData(ActorOp actor, StaticAnalysisData &data) {

    // fill out infos

    auto module = actor->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    auto edge_ops = actor.getOps<EdgeOp>() | IntoVector();
    auto node_ops = actor.getOps<NodeOp>() | IntoVector();

    std::vector<NodeCodegenData> node_codegen_datas;
    std::vector<EdgeCodegenData> edge_codegen_datas;

    DenseMap<NodeOp, Vec<VirtualFIFO_Edge *>> input_fifos;
    DenseMap<NodeOp, Vec<VirtualFIFO_Edge *>> output_fifos;
    DenseMap<EdgeOp, ArrayRef<char>> delay_data;

    std::vector<char> dummy_ptrs(node_ops.size(), ' ');

    [[maybe_unused]] auto &x = llvm::errs();

    for (auto [i, node] : llvm::enumerate(node_ops)) {
      auto &codegen_data = node_codegen_datas.emplace_back();
      codegen_data.index = i;
      codegen_data.node_op = node;
      codegen_data.static_info = data.node_static_info[node];
    }

    for (auto [i, edge] : llvm::enumerate(edge_ops)) {
      auto &codegen_data = edge_codegen_datas.emplace_back();
      codegen_data.index = i;
      codegen_data.edge_op = edge;
      codegen_data.static_info = data.edge_static_info[edge];
    }

    // validate the ptrs

    for (auto [i, node_pair] : llvm::enumerate(node_codegen_datas)) {
      node_pair.wrapper =
          getOrCodegenNodeWrapper(module, mod_builder, node_pair);
    }

    auto codegen_builder = CodegenStaticData(
        module, mod_builder, {node_codegen_datas}, {edge_codegen_datas});

    codegen_builder.codegenStaticData();

    auto actors = module.getOps<ActorOp>() | IntoVector();

    for (auto actor : actors) {
      actor->erase();
    }

    return success();
  }

  LogicalResult runOnOperation(ModuleOp module) {
    auto main_actor = getMainActor(module);

    upgradeDelaysToDenseArrays(main_actor);

    breakLoops(main_actor);

    auto static_analysis = sdf::analyzeAndAnnotate(main_actor);
    bool ok = llvm::succeeded(static_analysis);

    ok = ok && generateAllocsAndFrees(main_actor, *static_analysis).succeeded();

    main_actor->dump();

    ok = ok && codegenStaticData(main_actor, *static_analysis).succeeded();
    return success(ok);
  }
};

void VirtualFIFOSchedulerPass::runOnOperation() {
  pimpl = new Impl{this};
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}

} // namespace iara::passes::virtualfifo
