#include "Iara/IaraOps.h"
#include "Iara/Passes/FIFOScheduler/DynamicFIFOSchedulerPass.h"
#include "IaraRuntime/DynamicQueueScheduler.h"
#include "Util/Canon.h"
#include "Util/MlirUtil.h"
#include "Util/OpCreateHelper.h"
#include "Util/RangeUtil.h"
#include "Util/SDF.h"
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
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LogicalResult.h>

using namespace mlir::iara::rangeutil;
using namespace mlir::iara::canon;
using namespace mlir::iara::sdf;
using namespace mlir::iara::mlir_util;

namespace mlir::iara::passes::fifo {

using namespace func;

template <typename T> struct GetMLIRType {
  static Type get(MLIRContext *context) {
    llvm_unreachable("Unsupported");
    return nullptr;
  }
};

template <typename T> struct GetMLIRType<T &> {
  static Type get(MLIRContext *context) { return GetMLIRType<T>::get(context); }
};

template <> struct GetMLIRType<i64> {
  static Type get(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }
};

template <> struct GetMLIRType<LLVM::GlobalOp> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <> struct GetMLIRType<SymbolOpInterface> {
  static Type get(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }
};

template <typename T> Value asValue(OpBuilder builder, Location loc, T t) {
  return CREATE(LLVM::ConstantOp, builder, loc, asAttr(builder.getContext(), t))
      .getResult();
}

template <>
Value asValue<std::nullptr_t>(OpBuilder builder, Location loc, std::nullptr_t) {
  return CREATE(LLVM::ZeroOp, builder, loc,
                LLVM::LLVMPointerType::get(builder.getContext()))
      .getResult();
}

template <>
Value asValue<LLVM::GlobalOp>(OpBuilder builder, Location loc,
                              LLVM::GlobalOp global_op) {
  if (global_op)
    return CREATE(LLVM::AddressOfOp, builder, loc,
                  LLVM::LLVMPointerType::get(builder.getContext()),
                  global_op.getSymName())
        .getResult();
  else
    return asValue(builder, loc, nullptr);
}

template <>
Value asValue<SymbolOpInterface>(OpBuilder builder, Location loc,
                                 SymbolOpInterface op) {
  if (op)
    return CREATE(LLVM::AddressOfOp, builder, loc,
                  LLVM::LLVMPointerType::get(builder.getContext()),
                  op.getName())
        .getResult();
  else
    return asValue(builder, loc, nullptr);
}

struct DynamicPushFirstFIFOSchedulerPass::Impl {

  DenseMap<EdgeOp, Value> runtime_fifo_pointers;
  DenseMap<EdgeOp, LLVM::GlobalOp> edge_info_global_ops;
  DenseMap<NodeOp, LLVM::GlobalOp> node_infos;
  DenseMap<EdgeOp, LLVM::GlobalOp> delay_values;
  Type i64ype;
  Type pointer_type;
  Type void_type;

  template <typename T, typename... Args>
  void fillTypeVector(MLIRContext *context, SmallVector<Type> &vec) {
    vec.push_back(GetMLIRType<T>::get(context));
    if constexpr (sizeof...(Args) > 0) {
      fillTypeVector<Args...>(context, vec);
    }
  }

  template <typename... Args>
  LLVM::LLVMStructType getLLVMStructType(MLIRContext *context) {
    SmallVector<Type> types;
    fillTypeVector<Args...>(context, types);
    return LLVM::LLVMStructType::getLiteral(context, types);
  }

  // template <typename T> Value asValue(OpBuilder builder, Location loc, T t) {
  //   return asValue(builder, loc, t);
  // }

  template <typename T, typename... Args>
  Value fillOutContainer(OpBuilder builder, Value container, i64 position, T t,
                         Args... args) {
    auto loc = container.getDefiningOp()->getLoc();
    auto insert = CREATE(LLVM::InsertValueOp, builder, loc, container,
                         asValue(builder, loc, t), position);
    if constexpr (sizeof...(Args) > 0) {
      return fillOutContainer(builder, insert.getResult(), position + 1,
                              args...);
    }
    return insert.getResult();
  }

  template <typename... Args>
  LLVM::GlobalOp createGlobalStruct(OpBuilder builder, Location loc,
                                    StringRef name, Args... args) {
    auto type = getLLVMStructType<Args...>(builder.getContext());
    LLVM::GlobalOp rv = CREATE(LLVM::GlobalOp, builder, loc, type, false,
                               LLVM::Linkage::External, name, Attribute{});
    rv.getInitializerRegion().emplaceBlock();
    auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
    auto _struct = CREATE(LLVM::UndefOp, global_builder, loc, type).getResult();
    _struct = fillOutContainer(global_builder, _struct, 0, args...);
    CREATE(LLVM::ReturnOp, global_builder, loc, _struct);
    return rv;
  }

  LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
                                         DenseArrayAttr array, StringRef name,
                                         bool is_constant) {
    if (array.empty())
      return nullptr;
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    return CREATE(
        LLVM::GlobalOp, mod_builder, loc,
        LLVM::LLVMArrayType::get(array.getElementType(), array.getSize()),
        is_constant, LLVM::Linkage::External, name, array);
  };

  // Returns null if the range is empty.
  template <typename Range>
  LLVM::GlobalOp createGlobalArrayOrNull(ModuleOp module, Location loc,
                                         Range &&range, StringRef name,
                                         bool is_constant) {
    if (range.empty())
      return nullptr;
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto elem_type =
        GetMLIRType<decltype(*range.begin())>::get(module.getContext());
    auto items = llvm::to_vector(range);
    auto array_type = LLVM::LLVMArrayType::get(elem_type, items.size());
    LLVM::GlobalOp rv =
        CREATE(LLVM::GlobalOp, mod_builder, loc, array_type, is_constant,
               LLVM::Linkage::External, name, Attribute{});
    rv.getInitializer().emplaceBlock();
    auto global_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
    auto array_value =
        CREATE(LLVM::UndefOp, global_builder, loc, array_type).getResult();
    for (auto [i, v] : llvm::enumerate(items)) {
      array_value = CREATE(LLVM::InsertValueOp, global_builder, loc,
                           array_value, asValue(global_builder, loc, v), i)
                        .getResult();
    }
    CREATE(LLVM::ReturnOp, global_builder, loc, array_value);
    return rv;
  };

  void codegenFIFO(OpBuilder main_func_builder, EdgeInfo edge) {
    auto module = edge.op->getParentOfType<ModuleOp>();
    auto create_call =
        CREATE(CallOp, main_func_builder, edge.op->getLoc(),
               "iara_fifo_runtime_DynamicPushFirstFifo_create",
               {LLVM::LLVMPointerType::get(edge.op->getContext())});

    getOrGenFuncDecl(create_call, true);

    this->runtime_fifo_pointers[edge.op] = create_call->getResult(0);

    LLVM::GlobalOp delay_global{};
    if (auto array = llvm::dyn_cast_if_present<DenseArrayAttr>(edge.delay())) {
      auto sym_name = llvm::formatv("iara_edge_{0}_delay", edge.id()).str();
      delay_global = createGlobalArrayOrNull(module, edge.op->getLoc(), array,
                                             sym_name, true);
    }

    auto sym_name = llvm::formatv("iara_edge_info_{0}", edge.id()).str();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    // Runtime struct:
    // struct EdgeInfo {
    //   i64 prod_rate; // [0]
    //   i64 cons_rate; // [1]
    //   i64 delay_size; // [2]
    //   byte *initial_delays; // [3]
    //   DynamicPushFirstFifo *fifo; // [4] <-- here
    // };

    constexpr i64 fifo_index = 4;

    auto global_op =
        createGlobalStruct<i64, i64, i64, LLVM::GlobalOp, LLVM::GlobalOp>(
            mod_builder, edge.op.getLoc(), sym_name, edge.prod_rate(),
            edge.cons_rate(), edge.delay_size_bytes(), delay_global,
            LLVM::GlobalOp{} // start empty, to be filled in at initialization
        );

    edge_info_global_ops[edge.op] = global_op;

    // write pointer to fifo in global op

    auto global_address = CREATE(LLVM::AddressOfOp, main_func_builder,
                                 edge.op.getLoc(), global_op);
    auto global_struct = CREATE(LLVM::LoadOp, main_func_builder,
                                edge.op.getLoc(), global_address);
    auto inserted =
        CREATE(LLVM::InsertValueOp, main_func_builder, edge.op.getLoc(),
               global_struct, create_call.getResult(0), {fifo_index});
    CREATE(LLVM::StoreOp, main_func_builder, edge.op.getLoc(), inserted,
           global_address);
  }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  // The single argument is a pointer to a buffer of pointers to the memory
  // accessed by the kernel.
  LLVM::LLVMFuncOp createNodeWrapper(NodeInfo node) {
    auto module = node.op->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto sym_name = llvm::formatv("iara_node_wrapper_{0}", node.id()).str();

    auto opaque_pointer_type = LLVM::LLVMPointerType::get(module->getContext());
    auto pointer_to_opaque_pointer_type =
        LLVM::LLVMPointerType::get(opaque_pointer_type);

    SmallVector<Type> arg_types = {pointer_to_opaque_pointer_type};

    // for (auto i : node.op.getIn())
    //   arg_types.push_back(i.getType());

    // // (getOut returns both inout and pure outs.)
    // for (auto i : node.op.getOut())
    //   arg_types.push_back(i.getType());

    auto context = node.op->getContext();

    auto wrapper = CREATE(
        LLVM::LLVMFuncOp, mod_builder, node.op.getLoc(), sym_name,
        LLVM::LLVMFunctionType::get(context, LLVM::LLVMVoidType::get(context),
                                    pointer_to_opaque_pointer_type, false));

    wrapper.setVisibility(mlir::SymbolTable::Visibility::Public);
    wrapper->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());

    // addEntryBlock broken?

    auto *block = &wrapper.getFunctionBody().emplaceBlock();
    for (auto type : arg_types) {
      block->addArgument(type, wrapper->getLoc());
    }
    //   auto block = wrapper.addEntryBlock();

    auto func_builder = OpBuilder::atBlockBegin(block);

    // kernel must have format void name(params*, in*, inout*, out*).

    SmallVector<Value> kernel_args;

    for (auto p : node.op.getParams()) {
      auto constant = dyn_cast<arith::ConstantOp>(p.getDefiningOp());
      assert(constant && "Params should have been uniqued by now.");
      auto new_const = func_builder.clone(*constant);
      kernel_args.push_back(new_const->getResult(0));
    }

    auto num_buffers = node.op.getIn().size() + node.op.getOut().size();

    for (size_t i = 0; i < num_buffers; i++) {
      auto pointer_pointer =
          CREATE(LLVM::GEPOp, func_builder, node.op.getLoc(),
                 pointer_to_opaque_pointer_type,
                 func_builder.getBlock()->getArgument(0), {i});
      auto pointer =
          CREATE(LLVM::LoadOp, func_builder, node.op.getLoc(), pointer_pointer);
      kernel_args.push_back(pointer);
    }

    auto kernel_call = CREATE(CallOp, func_builder, node.op.getLoc(),
                              node.op.getImpl(), {}, kernel_args);

    getOrGenFuncDecl(kernel_call, true);

    CREATE(LLVM::ReturnOp, func_builder, node.op.getLoc(), ValueRange{});

    return wrapper;
  }

  LLVM::GlobalOp generateNodeInfo(NodeInfo node) {
    // matches NodeInfo in IaraRuntime/DynamicPushFirstFIFO.h
    auto module = node.op->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    auto sym_name = llvm::formatv("iara_node_info_{0}", node.id()).str();

    auto input_edge_infos = Vec<LLVM::GlobalOp>();

    // struct NodeInfo {
    //   i64 total_firings;
    //   i64 num_pure_inputs;
    //   i64 num_inouts;
    //   i64 num_pure_outputs;
    //   EdgeInfo *input_edges;
    //   EdgeInfo *output_edges;

    //   void (*kernel_wrapper)(void *args);
    // };

    for (auto input : node.op.getAllInputs()) {
      EdgeOp edge = dyn_cast<EdgeOp>(input.getDefiningOp());
      auto annotated = EdgeInfo(edge);
      input_edge_infos.push_back(edge_info_global_ops[annotated.op]);
    }

    auto inputs_sym_name =
        llvm::formatv("iara_node_info_{0}_inputs", node.id()).str();
    auto input_infos_array = createGlobalArrayOrNull(
        module, node.op.getLoc(), input_edge_infos, inputs_sym_name, true);

    auto output_edge_infos =
        node.op.getAllOutputs() | Map{[](auto v) -> EdgeInfo {
          return EdgeInfo(cast<EdgeOp>(*v.getUsers().begin()));
        }} |
        Map([&](EdgeInfo edge) { return edge_info_global_ops[edge.op]; });

    auto outputs_sym_name =
        llvm::formatv("iara_node_info_{0}_outputs", node.id()).str();
    auto output_infos_array = createGlobalArrayOrNull(
        module, node.op.getLoc(), output_edge_infos, outputs_sym_name, true);

    auto node_wrapper = createNodeWrapper(node);

    auto node_info = createGlobalStruct<i64, i64, i64, i64, LLVM::GlobalOp,
                                        LLVM::GlobalOp, SymbolOpInterface>(
        mod_builder, node.op.getLoc(), sym_name, node.total_firings(),
        node.op.getIn().size(), node.op.getInout().size(),
        node.op.getPureOuts().size(), input_infos_array, output_infos_array,
        node_wrapper);

    node_infos[node.op] = node_info;

    return node_info;
  }

  void codegenNode(OpBuilder builder, NodeInfo node) {
    auto info = generateNodeInfo(node);
    node_infos[node.op] = info;

    auto info_ptr = CREATE(LLVM::AddressOfOp, builder, node.op->getLoc(), info);

    auto firings_call = CREATE(CallOp, builder, node.op->getLoc(),
                               "iara_fire_node", {}, {info_ptr});
    getOrGenFuncDecl(firings_call, true);
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
      module->emitError(
          "At this point, the graph should have had only one definition; there "
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

  FuncOp convertToFunction(ActorOp actor) {
    auto [main_func, main_func_builder] =
        mlir_util::createEmptyVoidFunctionWithBody(
            OpBuilder(actor), actor.getSymName(), actor.getLoc());

    // only once:

    auto edges = actor.getOps<EdgeOp>() | IntoVector();
    auto nodes = actor.getOps<NodeOp>() | IntoVector();

    for (auto edge_op : actor.getOps<EdgeOp>()) {
      codegenFIFO(main_func_builder, EdgeInfo(edge_op));
    }
    for (auto node_op : actor.getOps<NodeOp>()) {
      codegenNode(main_func_builder, NodeInfo(node_op));
    }

    CREATE(ReturnOp, main_func_builder, main_func.getLoc(), {});

    actor->erase();

    return main_func;
  }

  LogicalResult runOnOperation(ModuleOp module) {
    auto main_actor = getMainActor(module);
    if (canon::canonicalize(main_actor).failed()) {
      return failure();
    };
    if (sdf::analyzeAndAnnotate(main_actor).failed())
      return failure();
    i64ype = OpBuilder(module).getI64Type();
    pointer_type = LLVM::LLVMPointerType::get(module->getContext());
    void_type = LLVM::LLVMVoidType::get(module.getContext());
    convertToFunction(main_actor);
    return success();
  }
};

void DynamicPushFirstFIFOSchedulerPass::runOnOperation() {
  pimpl = new Impl();
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}

} // namespace mlir::iara::passes::fifo
