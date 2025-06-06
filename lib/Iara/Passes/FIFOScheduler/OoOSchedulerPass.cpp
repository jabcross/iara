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
        llvm_void_type(),
        {IntegerType::get(ctx(), 64), pointer_to(pointer_to(chunk_type()))},
        false);
  }

  // template <typename T, typename... Args>
  // void fillTypeVector(MLIRContext *context, SmallVector<Type> &vec) {
  //   vec.push_back(GetMLIRType<T>::get(context));
  //   if constexpr (sizeof...(Args) > 0) {
  //     fillTypeVector<Args...>(context, vec);
  //   }
  // }

  // template <typename... Args>
  // LLVM::LLVMStructType getLLVMStructType(MLIRContext *context) {
  //   SmallVector<Type> types;
  //   fillTypeVector<Args...>(context, types);
  //   return LLVM::LLVMStructType::getLiteral(context, types);
  // }

  // // template <typename T> Value asValue(OpBuilder builder, Location loc, T
  // t) {
  // //   return asValue(builder, loc, t);
  // // }

  // template <typename T, typename... Args>
  // Value fillOutContainer(OpBuilder builder, Value container, i64 position, T
  // t,
  //                        Args... args) {
  //   auto loc = container.getDefiningOp()->getLoc();
  //   auto insert = CREATE(LLVM::InsertValueOp, builder, loc, container,
  //                        asValue(builder, loc, t), position);
  //   if constexpr (sizeof...(Args) > 0) {
  //     return fillOutContainer(builder, insert.getResult(), position + 1,
  //                             args...);
  //   }
  //   return insert.getResult();
  // }

  // template <typename... Args>
  // LLVM::GlobalOp createGlobalStruct(OpBuilder builder, Location loc,
  //                                   StringRef name, Args... args) {
  //   auto type = getLLVMStructType<Args...>(builder.getContext());
  //   LLVM::GlobalOp rv = CREATE(LLVM::GlobalOp, builder, loc, type, false,
  //                              LLVM::Linkage::External, name, Attribute{});
  //   rv.getInitializerRegion().emplaceBlock();
  //   auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
  //   auto _struct = CREATE(LLVM::UndefOp, global_builder, loc,
  //   type).getResult(); _struct = fillOutContainer(global_builder, _struct, 0,
  //   args...); CREATE(LLVM::ReturnOp, global_builder, loc, _struct); return
  //   rv;
  // }

  // LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
  //                                        DenseArrayAttr array, StringRef
  //                                        name, bool is_constant) {
  //   if (array.empty())
  //     return nullptr;
  //   auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
  //   return CREATE(
  //       LLVM::GlobalOp, mod_builder, loc,
  //       LLVM::LLVMArrayType::get(array.getElementType(), array.getSize()),
  //       is_constant, LLVM::Linkage::External, name, array);
  // };

  // // Returns null if the range is empty.
  // template <typename Range>
  // LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
  //                                        Range &&range, StringRef name,
  //                                        bool is_constant) {
  //   if (range.empty())
  //     return nullptr;
  //   auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
  //   auto elem_type =
  //       GetMLIRType<decltype(*range.begin())>::get(module.getContext());
  //   auto items = llvm::to_vector(range);
  //   auto array_type = LLVM::LLVMArrayType::get(elem_type, items.size());
  //   LLVM::GlobalOp rv =
  //       CREATE(LLVM::GlobalOp, mod_builder, loc, array_type, is_constant,
  //              LLVM::Linkage::External, name, Attribute{});
  //   rv.getInitializer().emplaceBlock();
  //   auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
  //   auto array_value =
  //       CREATE(LLVM::UndefOp, global_builder, loc, array_type).getResult();
  //   for (auto [i, v] : llvm::enumerate(items)) {
  //     array_value = CREATE(LLVM::InsertValueOp, global_builder, loc,
  //                          array_value, asValue(global_builder, loc, v), i)
  //                       .getResult();
  //   }
  //   CREATE(LLVM::ReturnOp, global_builder, loc, array_value);
  //   return rv;
  // };

  // void codegenFIFO(OpBuilder main_func_builder, SDFEdge edge) {
  //   auto module = edge->getParentOfType<ModuleOp>();
  //   auto create_call = CREATE(CallOp, main_func_builder, edge->getLoc(),
  //                             "iara_fifo_runtime_DynamicPushFirstFifo_create",
  //                             {LLVM::LLVMPointerType::get(edge->getContext())});

  //   getOrGenFuncDecl(create_call, true);

  //   this->runtime_fifo_pointers[edge] = create_call->getResult(0);

  //   LLVM::GlobalOp delay_global{};
  //   if (auto array = llvm::dyn_cast_if_present<DenseArrayAttr>(edge.delay()))
  //   {
  //     auto sym_name = llvm::formatv("iara_edge_{0}_delay", edge.id()).str();
  //     delay_global = createGlobalArrayOrNull(module, edge->getLoc(), array,
  //                                            sym_name, true);
  //   }

  //   auto sym_name = llvm::formatv("iara_edge_info_{0}", edge.id()).str();
  //   auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

  //   // Runtime struct:
  //   // struct EdgeInfo {
  //   //   i64 prod_rate; // [0]
  //   //   i64 cons_rate; // [1]
  //   //   i64 delay_size; // [2]
  //   //   byte *initial_delays; // [3]
  //   //   DynamicPushFirstFifo *fifo; // [4] <-- here
  //   // };

  //   constexpr i64 fifo_index = 4;

  //   auto global_op =
  //       createGlobalStruct<i64, i64, i64, LLVM::GlobalOp, LLVM::GlobalOp>(
  //           mod_builder, edge.getLoc(), sym_name, edge.prod_rate(),
  //           edge.cons_rate(), edge.delay_size_bytes(), delay_global,
  //           LLVM::GlobalOp{} // start empty, to be filled in at
  //           initialization
  //       );

  //   edge_info_global_ops[edge] = global_op;

  //   // write pointer to fifo in global op

  //   auto global_address =
  //       CREATE(LLVM::AddressOfOp, main_func_builder, edge.getLoc(),
  //       global_op);
  //   auto global_struct =
  //       CREATE(LLVM::LoadOp, main_func_builder, edge.getLoc(),
  //       global_address);
  //   auto inserted =
  //       CREATE(LLVM::InsertValueOp, main_func_builder, edge.getLoc(),
  //              global_struct, create_call.getResult(0), {fifo_index});
  //   CREATE(LLVM::StoreOp, main_func_builder, edge.getLoc(), inserted,
  //          global_address);
  // }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  // The single argument is a pointer to a buffer of pointers to the memory
  // accessed by the kernel.
  LLVM::LLVMFuncOp codegenNodeWrapper(NodeOp node, SDF_OoO_Node &info) {
    auto module = node->getParentOfType<ModuleOp>();
    if (node.isAlloc()) {
      return getLLVMFuncDecl(module, "iara_runtime_alloc", wrapper_type());
    }
    if (node.isDealloc()) {
      return getLLVMFuncDecl(module, "iara_runtime_dealloc", wrapper_type());
    }

    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto sym_name = llvm::formatv("iara_node_wrapper_{0}", info.info.id).str();

    auto ctx = module.getContext();

    auto num_buffers = node.getIn().size() + node.getOut().size();

    auto opaque_ptr_type = LLVM::LLVMPointerType::get(ctx);
    auto pointer_to_chunk_type = pointer_to(chunk_type());
    auto pointer_to_pointer_to_chunk_type = pointer_to(pointer_to_chunk_type);
    auto pointer_to_opaque_pointer_type = pointer_to(opaque_ptr_type);

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
          CREATE(LLVM::GEPOp, func_builder, node.getLoc(),
                 pointer_to_pointer_to_chunk_type, pointer_to_chunk_type,
                 func_builder.getBlock()->getArgument(1), {i});

      auto pointer_to_chunk = CREATE(LLVM::LoadOp, func_builder, node.getLoc(),
                                     pointer_to_pointer_to_chunk);

      // get first value
      auto pointer_to_opaque_pointer =
          CREATE(LLVM::GEPOp, func_builder, node.getLoc(),
                 pointer_to_opaque_pointer_type, pointer_to_chunk,
                 getIntConstant(func_builder.getInsertionBlock(), 0), true);

      auto opaque_pointer = CREATE(LLVM::LoadOp, func_builder, node.getLoc(),
                                   pointer_to_opaque_pointer);

      kernel_args.push_back(opaque_pointer);
    }

    auto kernel_call = CREATE(CallOp, func_builder, node.getLoc(),
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

    // only once:

    auto edges = actor.getOps<EdgeOp>() | IntoVector();
    auto nodes = actor.getOps<NodeOp>() | IntoVector();

    DenseMap<NodeOp, size_t> runtime_node_data_index;
    DenseMap<EdgeOp, SDF_OoO_FIFO *> runtime_edge_data;

    DenseMap<NodeOp, Vec<void *>> output_fifos;
    DenseMap<EdgeOp, ArrayRef<char>> delay_data;

    std::map<void *, std::string> known_global_ptrs;

    std::vector<char> dummy_ptrs(nodes.size(), ' ');

    std::vector<SDF_OoO_Node> _node_infos{[&]() {
      std::vector<SDF_OoO_Node> node_infos;
      for (auto [i, node] : llvm::enumerate(nodes)) {
        auto &info = node_infos.emplace_back();
        runtime_node_data_index[node] = node_infos.size() - 1;
        info.info = data.node_static_info[node];
        info.wrapper = (SDF_OoO_Node::WrapperType *)&dummy_ptrs[i];
        info.semaphore_map = nullptr;
        known_global_ptrs[(void *)info.wrapper] =
            codegenNodeWrapper(node, info).getSymName().str();
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
            module, &runtime_data, "iara_runtime_data", known_global_ptrs,
            actor.getLoc());
    for (auto [i, node, info] : llvm::enumerate(nodes, node_infos)) {
      auto func = getLLVMFuncDecl(
          node->getParentOfType<ModuleOp>(), "iara_runtime_node_init",
          LLVM::LLVMFunctionType::get(llvm_void_type(),
                                      {LLVM::LLVMPointerType::get(ctx())}));
      func.setPrivate();

      auto [offset, name] = static_builder.pointer_lists[(void *)&info];
      auto ptr =
          static_builder.regenRuntimePointer(main_func_builder, (void *)&info);

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
