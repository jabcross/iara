#include "Iara/Codegen/Codegen.h"
#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/FIFOScheduler/OoOSchedulerPass.h"
#include "Iara/SDF/Canon.h"
#include "Iara/SDF/SDF.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/DynamicQueueScheduler.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "IaraRuntime/SDF_OoO_Scheduler.h"
#include <cstddef>
#include <iterator>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Conversion/LLVMCommon/StructBuilder.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/Dialect/LLVMIR/Transforms/Passes.h>
#include <mlir/Dialect/LLVMIR/Transforms/TypeConsistency.h>
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
#include <mlir/Support/LogicalResult.h>

using namespace iara::util::range;
using namespace iara::sdf::canon;
using namespace iara::sdf;
using namespace iara::util::mlir;

namespace iara::passes::fifo {

using namespace func;
using namespace iara::codegen;

struct OoOSchedulerPass::Impl {
  OoOSchedulerPass *pass;
  DenseMap<EdgeOp, Value> runtime_fifo_pointers;
  DenseMap<EdgeOp, LLVM::GlobalOp> edge_info_global_ops;
  DenseMap<NodeOp, LLVM::GlobalOp> node_infos;
  DenseMap<EdgeOp, LLVM::GlobalOp> delay_values;

  Impl(OoOSchedulerPass *pass) : pass(pass) {}

  MLIRContext *ctx() { return &pass->getContext(); }

  Type i64type() { return IntegerType::get(ctx(), 64); }

  LLVM::LLVMPointerType llvm_pointer_type() {
    return LLVM::LLVMPointerType::get(ctx());
  }
  LLVM::LLVMVoidType llvm_void_type() { return LLVM::LLVMVoidType::get(ctx()); }

  LLVM::LLVMStructType chunk_type() {
    return cast<LLVM::LLVMStructType>(getMLIRType<Chunk>(ctx()));
  }

  LLVM::LLVMPointerType pointer_to(Type type) {
    return LLVM::LLVMPointerType::get(type, 0);
  }

  LLVM::LLVMFunctionType wrapper_type() {
    return LLVM::LLVMFunctionType::get(
        llvm_void_type(), {IntegerType::get(ctx(), 64), llvm_pointer_type()},
        false);
  }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  // The single argument is a pointer to a buffer of pointers to the memory
  // accessed by the kernel.
  LLVM::LLVMFuncOp getOrCodegenNodeWrapper(ModuleOp module,
                                           OpBuilder mod_builder, NodeOp node,
                                           SDF_OoO_Node &info) {

    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx());
    if (node.isAlloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_alloc")) {
        return existing;
      }
      return CREATE(LLVM::LLVMFuncOp, mod_builder, module.getLoc(),
                    "iara_runtime_alloc", wrapper_type());
    }
    if (node.isDealloc()) {
      if (auto existing =
              module.lookupSymbol<LLVM::LLVMFuncOp>("iara_runtime_delloc")) {
        return existing;
      }
      return CREATE(LLVM::LLVMFuncOp, mod_builder, module.getLoc(),
                    "iara_runtime_dealloc", wrapper_type());
    }

    auto sym_name = llvm::formatv("iara_node_wrapper_{0}", info.info.id).str();

    if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(sym_name)) {
      return existing;
    }

    auto ctx = module.getContext();

    auto num_buffers = node.getIn().size() + node.getOut().size();

    auto pointer_to_chunk_type = pointer_to(chunk_type());
    auto pointer_to_pointer_to_chunk_type = pointer_to(pointer_to_chunk_type);
    // auto pointer_to_opaque_pointer_type = pointer_to(opaque_ptr_type);

    // for (auto i : node.op.getIn())
    //   arg_types.push_back(i.getType());

    // // (getOut returns both inout and pure outs.)
    // for (auto i : node.op.getOut())
    //   arg_types.push_back(i.getType());

    auto wrapper = CREATE(LLVM::LLVMFuncOp, mod_builder, node.getLoc(),
                          sym_name, wrapper_type());

    wrapper.setVisibility(mlir::SymbolTable::Visibility::Public);
    wrapper->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());

    // addEntryBlock broken?

    auto *block = &wrapper.getFunctionBody().emplaceBlock();
    for (auto type : wrapper_type().getParams()) {
      block->addArgument(type, wrapper->getLoc());
    }

    //   auto block = wrapper.addEntryBlock();

    auto func_builder = OpBuilder::atBlockBegin(block);

    // kernel must have format void name(params*, in*, inout*, out*).

    SmallVector<Value> kernel_args;

    for (auto p : node.getParams()) {
      auto constant = dyn_cast<arith::ConstantOp>(p.getDefiningOp());
      assert(constant && "Params should have been uniqued by now.");
      auto new_const = func_builder.clone(*constant);
      kernel_args.push_back(new_const->getResult(0));
    }

    for (size_t i = 0; i < num_buffers; i++) {
      // Value cast = CREATE(LLVM::BitcastOp, func_builder, node.getLoc(),
      //                     pointer_to_chunk_type,
      //                     func_builder.getBlock()->getArgument(1));

      // get pointer from array
      auto pointer_to_pointer_to_chunk =
          CREATE(LLVM::GEPOp, func_builder, node.getLoc(), opaque_ptr_type,
                 pointer_to_pointer_to_chunk_type,
                 func_builder.getBlock()->getArgument(1), {i});

      auto pointer_to_chunk =
          CREATE(LLVM::LoadOp, func_builder, node.getLoc(), opaque_ptr_type,
                 pointer_to_pointer_to_chunk);

      // get first value

      auto pointer_to_opaque_pointer =
          CREATE(LLVM::GEPOp, func_builder, node.getLoc(), opaque_ptr_type,
                 pointer_to_chunk_type, pointer_to_chunk,
                 getIntConstant(func_builder.getInsertionBlock(), 0));

      auto opaque_pointer = CREATE(LLVM::LoadOp, func_builder, node.getLoc(),
                                   opaque_ptr_type, pointer_to_opaque_pointer);

      kernel_args.push_back(opaque_pointer);
    }

    auto kernel_call = CREATE(LLVM::CallOp, func_builder, node.getLoc(),
                              TypeRange{}, node.getImpl(), kernel_args);

    getOrGenFuncDecl(kernel_call);
    CREATE(LLVM::ReturnOp, func_builder, node.getLoc(), ValueRange{});

    return wrapper;
  }

  ActorOp getMainActor(ModuleOp module) {
    // At this point, there should be only one actor with a body.

    Vec<ActorOp> definitions;

    for (auto actor : module.getOps<ActorOp>()) {
      if (actor.isKernelDeclaration()) {
        continue;
      }
      definitions.push_back(actor);
    }
    if (definitions.size() > 1) {
      module->emitError("At this point, the graph should have had only one "
                        "definition; there "
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

  void codegenEdgeInit(OpBuilder builder, EdgeOp edge, SDF_OoO_FIFO &info) {}

  FailureOr<FuncOp> convertToFunction(ActorOp actor, StaticAnalysisData &data) {
    auto [main_func, main_func_builder] = createEmptyVoidFunctionWithBody(
        OpBuilder(actor), actor.getSymName(), actor.getLoc());

    // fill out infos

    auto module = actor->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    auto edges = actor.getOps<EdgeOp>() | IntoVector();
    auto nodes = actor.getOps<NodeOp>() | IntoVector();

    DenseMap<NodeOp, size_t> runtime_node_data_index;
    DenseMap<EdgeOp, size_t> runtime_edge_data_index;
    DenseMap<EdgeOp, SDF_OoO_FIFO *> runtime_edge_data;

    DenseMap<NodeOp, Vec<SDF_OoO_FIFO *>> output_fifos;
    DenseMap<EdgeOp, ArrayRef<char>> delay_data;

    std::vector<char> dummy_ptrs(nodes.size(), ' ');

    [[maybe_unused]] auto &x = llvm::errs();

    std::vector<SDF_OoO_Node> _node_infos;
    node_infos.reserve(nodes.size());

    for (auto [i, node] : llvm::enumerate(nodes)) {
      auto &info = _node_infos.emplace_back();
      runtime_node_data_index[node] = _node_infos.size() - 1;
      info.info = data.node_static_info[node];
      info.wrapper = (SDF_OoO_Node::WrapperType *)&dummy_ptrs[i];
      info.semaphore_map = nullptr;
    }

    std::span<SDF_OoO_Node> node_infos(_node_infos);

    std::vector<SDF_OoO_FIFO> _edge_infos;
    _edge_infos.reserve(edges.size());

    for (auto [i, edge] : llvm::enumerate(edges)) {
      auto &info = _edge_infos.emplace_back();
      runtime_edge_data[edge] = &info;
      runtime_edge_data_index[edge] = i;
      info.info = data.edge_static_info[edge];
      info.consumer =
          &node_infos[runtime_node_data_index[edge.getConsumerNode()]];
      info.producer =
          &node_infos[runtime_node_data_index[edge.getProducerNode()]];

      if (info.info.delay_size > 0) {
        auto delay = edge->getAttrOfType<DenseArrayAttr>("delay");
        delay_data[edge] = delay.getRawData();
        info.delay_data = Span<const char>{delay.getRawData().begin(),
                                           delay.getRawData().size()};
      } else {
        info.delay_data = {};
      }
    }

    std::span<SDF_OoO_FIFO> edge_infos{_edge_infos};

    for (auto [i, node, info] : llvm::enumerate(nodes, node_infos)) {
      output_fifos[node].reserve(
          node.getAllOutputs()
              .size()); // prevent it from moving around in memory?
      for (auto output : node.getAllOutputs()) {
        auto edge = cast<EdgeOp>(*output.getUsers().begin());
        output_fifos[node].push_back(runtime_edge_data[edge]);
      }
      info.output_fifos = Span<SDF_OoO_FIFO *>{output_fifos[node].begin(),
                                               output_fifos[node].size()};
    }

    std::vector<LLVM::LLVMFuncOp> wrappers;

    for (auto [i, node, info] : enumerate(nodes, node_infos)) {
      wrappers.push_back(
          getOrCodegenNodeWrapper(module, mod_builder, node, info));
    }

    auto codegenBuilder =
        codegen::CodegenStaticData(module, mod_builder, {node_infos},
                                   {edge_infos}, {nodes}, {edges}, {wrappers});

    auto node_info_ptrs =
        codegenBuilder.codegenStaticData()(main_func_builder, actor.getLoc());

    // Init nodes

    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx());
    auto init_func = CREATE(LLVM::LLVMFuncOp, mod_builder, module.getLoc(),
                            "iara_runtime_node_init",
                            LLVM::LLVMFunctionType::get(
                                LLVMVoidType::get(ctx()), {opaque_ptr_type}));

    for (auto [i, node, info, ptr] :
         enumerate(nodes, node_infos, node_info_ptrs)) {

      CREATE(LLVM::CallOp, main_func_builder, node.getLoc(), init_func, {ptr});
    }

    // kickstart allocs

    auto kickstart_func = CREATE(
        LLVM::LLVMFuncOp, mod_builder, module.getLoc(), "kickstart_alloc",
        LLVM::LLVMFunctionType::get(LLVMVoidType::get(ctx()),
                                    {opaque_ptr_type}));

    for (auto [i, node, info, ptr] :
         enumerate(nodes, node_infos, node_info_ptrs)) {
      if (!node.isAlloc())
        continue;

      CREATE(LLVM::CallOp, main_func_builder, node.getLoc(), kickstart_func,
             {ptr});
    }

    CREATE(LLVM::ReturnOp, main_func_builder, main_func.getLoc(), ValueRange{});

    actor->erase();
    return main_func;
  }

  LogicalResult runOnOperation(ModuleOp module) {
    auto main_actor = getMainActor(module);
    if (failed(sdf::canon::canonicalize(main_actor))) {
      return failure();
    };
    auto static_analysis = sdf::analyzeAndAnnotate(main_actor);
    return success(
        succeeded(static_analysis) &&
        succeeded(generateAllocsAndFrees(main_actor, *static_analysis)) &&
        succeeded(convertToFunction(main_actor, *static_analysis)));
  }
};

void OoOSchedulerPass::runOnOperation() {
  pimpl = new Impl{this};
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}

} // namespace iara::passes::fifo
