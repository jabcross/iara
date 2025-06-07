#include "Iara/Codegen/Codegen.h"
#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Codegen/StaticBuilder.h"
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
  LLVM::LLVMFuncOp getOrCodegenNodeWrapper(NodeOp node, SDF_OoO_Node &info) {
    auto module = node->getParentOfType<ModuleOp>();
    if (node.isAlloc()) {
      return getLLVMFuncDecl(module, "iara_runtime_alloc", wrapper_type());
    }
    if (node.isDealloc()) {
      return getLLVMFuncDecl(module, "iara_runtime_dealloc", wrapper_type());
    }

    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto sym_name = llvm::formatv("iara_node_wrapper_{0}", info.info.id).str();

    if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(sym_name)) {
      return existing;
    }

    auto ctx = module.getContext();

    auto num_buffers = node.getIn().size() + node.getOut().size();

    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx);
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
                              node.getImpl(), {}, kernel_args);

    getOrGenFuncDecl(kernel_call, true);
    CREATE(LLVM::ReturnOp, func_builder, node.getLoc(), ValueRange{});

    return wrapper;
  }

  // LLVM::GlobalOp generateNodeInfo(SDFNode node) {
  //   // matches NodeInfo in IaraRuntime/DynamicPushFirstFIFO.h
  //   auto module = node->getParentOfType<ModuleOp>();
  //   auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

  //   auto sym_name = llvm::formatv("iara_node_info_{0}", node.id()).str();

  //   auto input_edge_infos = Vec<LLVM::GlobalOp>();

  //   // struct NodeInfo {
  //   //   i64 total_firings;
  //   //   i64 num_pure_inputs;
  //   //   i64 num_inouts;
  //   //   i64 num_pure_outputs;
  //   //   EdgeInfo *input_edges;
  //   //   EdgeInfo *output_edges;

  //   //   void (*kernel_wrapper)(void *args);
  //   // };

  //   for (auto input : node.getAllInputs()) {
  //     EdgeOp edge = dyn_cast<EdgeOp>(input.getDefiningOp());
  //     auto annotated = SDFEdge(edge);
  //     input_edge_infos.push_back(edge_info_global_ops[annotated]);
  //   }

  //   auto inputs_sym_name =
  //       llvm::formatv("iara_node_info_{0}_inputs", node.id()).str();
  //   auto input_infos_array = createGlobalArrayOrNull(
  //       module, node.getLoc(), input_edge_infos, inputs_sym_name, true);

  //   auto output_edge_infos =
  //       node.getAllOutputs() | Map{[](auto v) -> SDFEdge {
  //         return SDFEdge(cast<EdgeOp>(*v.getUsers().begin()));
  //       }} |
  //       Map([&](SDFEdge edge) { return edge_info_global_ops[edge]; });

  //   auto outputs_sym_name =
  //       llvm::formatv("iara_node_info_{0}_outputs", node.id()).str();
  //   auto output_infos_array = createGlobalArrayOrNull(
  //       module, node.getLoc(), output_edge_infos, outputs_sym_name, true);

  //   auto node_wrapper = createNodeWrapper(node);

  //   auto node_info = createGlobalStruct<i64, i64, i64, i64, LLVM::GlobalOp,
  //                                       LLVM::GlobalOp, SymbolOpInterface>(
  //       mod_builder, node.getLoc(), sym_name, node.total_firings(),
  //       node.getIn().size(), node.getInout().size(),
  //       node.getPureOuts().size(), input_infos_array, output_infos_array,
  //       node_wrapper);

  //   node_infos[node] = node_info;

  //   return node_info;
  // }

  // void codegenNode(OpBuilder builder, SDFNode node) {
  //   auto info = generateNodeInfo(node);
  //   node_infos[node] = info;

  //   auto info_ptr = CREATE(LLVM::AddressOfOp, builder, node->getLoc(), info);

  //   auto firings_call = CREATE(CallOp, builder, node->getLoc(),
  //                              "iara_fire_node", {}, {info_ptr});
  //   getOrGenFuncDecl(firings_call, true);
  // }

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

    // test static emitter

    // struct Test {
    //   std::vector<int> *x;
    //   std::vector<int *> y;
    //   float z;
    // };

    // std::vector<int> v = {1, 2, 3};

    // Test z = {.x = &v, .y = {&v[0], &v[1], &v[2]}, .z = 3.0};

    // std::map<void *, std::string> test_known_global_ptrs;

    // StaticBuilder<Test>::build(actor->getParentOfType<ModuleOp>(), &z,
    //                            "test_static_data", actor->getLoc(),
    //                            test_known_global_ptrs);

    // only once:

    auto edges = actor.getOps<EdgeOp>() | IntoVector();
    auto nodes = actor.getOps<NodeOp>() | IntoVector();

    DenseMap<NodeOp, size_t> runtime_node_data_index;
    DenseMap<EdgeOp, SDF_OoO_FIFO *> runtime_edge_data;

    DenseMap<NodeOp, Vec<void *>> output_fifos;
    DenseMap<EdgeOp, ArrayRef<char>> delay_data;

    std::map<void *, std::string> known_global_ptrs;

    std::vector<char> dummy_ptrs(nodes.size(), ' ');

    [[maybe_unused]] auto &x = llvm::errs();

    std::vector<SDF_OoO_Node> _node_infos{[&]() {
      std::vector<SDF_OoO_Node> node_infos;
      for (auto [i, node] : llvm::enumerate(nodes)) {
        auto &info = node_infos.emplace_back();
        runtime_node_data_index[node] = node_infos.size() - 1;
        info.info = data.node_static_info[node];
        info.wrapper = (SDF_OoO_Node::WrapperType *)&dummy_ptrs[i];
        info.semaphore_map = nullptr;
        auto wrapper = getOrCodegenNodeWrapper(node, info);
        known_global_ptrs[(void *)info.wrapper] = wrapper.getSymName();
      }
      return node_infos;
    }()};
    std::span<SDF_OoO_Node> node_infos(_node_infos);

    std::vector<SDF_OoO_FIFO> _edge_infos = [&]() {
      std::vector<SDF_OoO_FIFO> edge_infos;
      for (auto [i, edge] : llvm::enumerate(edges)) {
        auto &info = edge_infos.emplace_back();
        runtime_edge_data[edge] = &info;
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
      return edge_infos;
    }();

    std::span<SDF_OoO_FIFO> edge_infos{_edge_infos};

    for (auto [i, node, info] : llvm::enumerate(nodes, node_infos)) {
      Vec<SDF_OoO_FIFO *> output_fifos;
      for (auto output : node.getAllOutputs()) {
        auto edge = cast<EdgeOp>(*output.getUsers().begin());
        output_fifos.push_back(runtime_edge_data[edge]);
      }
      info.output_fifos =
          Span<SDF_OoO_FIFO *>{output_fifos.begin(), output_fifos.size()};
    }

    auto module = actor->getParentOfType<ModuleOp>();

    SDF_OoO_RuntimeData runtime_data{
        .node_infos = {&*node_infos.begin(), node_infos.size()},
        .fifo_infos = {&*edge_infos.begin(), edge_infos.size()}};

    [[maybe_unused]] auto [runtime_data_global, static_builder] =
        codegen::StaticBuilder<SDF_OoO_RuntimeData>::build(
            module, &runtime_data, "iara_runtime_data", actor.getLoc(),
            known_global_ptrs);

    // Initialize nodes
    for (auto [i, node, info] : llvm::enumerate(nodes, node_infos)) {
      auto node_init_func = getLLVMFuncDecl(
          node->getParentOfType<ModuleOp>(), "iara_runtime_node_init",
          LLVM::LLVMFunctionType::get(llvm_void_type(),
                                      {LLVM::LLVMPointerType::get(ctx())}));
      node_init_func.setPrivate();

      // Type void_type = LLVM::LLVMVoidType::get(ctx());

      std::string name("iara_runtime_data__node_infos");

      auto ptr = static_builder.pointer_map[{
          &info, getMLIRType<SDF_OoO_Node>(ctx()), llvm::hash_value(name)}](
          main_func_builder, node.getLoc(), (void *)&info);

      CREATE(LLVM::CallOp, main_func_builder, node.getLoc(), node_init_func,
             {ptr});
    }

    // Schedule first executions
    for (auto [i, node, info] : llvm::enumerate(nodes, node_infos)) {
      if (!node.isAlloc())
        continue;

      auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx());

      std::string name("iara_runtime_data__node_infos");

      auto ptr = static_builder.pointer_map[{
          &info, getMLIRType<SDF_OoO_Node>(ctx()), llvm::hash_value(name)}](
          main_func_builder, node.getLoc(), (void *)&info);

      auto func = getLLVMFuncDecl(
          node->getParentOfType<ModuleOp>(), "kickstart_alloc",
          LLVM::LLVMFunctionType::get(llvm_void_type(), {opaque_ptr_type}));
      func.setPrivate();

      CREATE(LLVM::CallOp, main_func_builder, node.getLoc(), func, {ptr});
    }

    CREATE(ReturnOp, main_func_builder, main_func.getLoc(), {});

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
