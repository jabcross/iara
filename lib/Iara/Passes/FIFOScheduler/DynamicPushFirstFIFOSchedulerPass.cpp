#include "Iara/IaraOps.h"
#include "Iara/Passes/FIFOScheduler/DynamicFIFOSchedulerPass.h"
#include "IaraRuntime/DynamicPushFirstFIFOScheduler.h"
#include "IaraRuntime/DynamicQueueScheduler.h"
#include "Util/MlirUtil.h"
#include "Util/OpCreateHelper.h"
#include "Util/RangeUtil.h"
#include <cstddef>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Conversion/LLVMCommon/StructBuilder.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>

using namespace RangeUtil;
using namespace mlir::iara::mlir_util;
using mlir::presburger::IntMatrix;
using mlir::presburger::MPInt;
using util::Rational;

namespace mlir::iara::passes::fifo {

using namespace func;

using UnnannotatedNodeOp = mlir::iara::NodeOp;
using UnnannotatedEdgeOp = mlir::iara::EdgeOp;

struct NodeOp : UnnannotatedNodeOp {
  i64 total_firings() {
    auto attr = (*this)->getAttr("total_firings").dyn_cast<IntegerAttr>();
    if (!attr) {
      llvm_unreachable("This node was not processed correctly.");
    }
    return attr.getInt();
  };

  i64 id() { return (*this)["node_id"]; }

  static bool classof(UnnannotatedNodeOp N) {
    return ((NodeOp)N)->hasAttr("static_info");
  }
};

struct EdgeOp : UnnannotatedEdgeOp {
  static bool classof(UnnannotatedEdgeOp E) {
    return ((EdgeOp)E)->hasAttr("static_info");
  }
  i64 id() { return (*this)["edge_id"]; }
  i64 prod_rate() { return (*this)["prod_rate"]; }
  i64 cons_rate() { return (*this)["cons_rate"]; }
  i64 delay_size() { return (*this)["delay_size"]; }
  Attribute delay() { return (*this)["delay"]; }
};

struct DynamicPushFirstFIFOSchedulerPass::Impl {

  DenseMap<EdgeOp, Value> runtime_fifo_pointers;
  DenseMap<EdgeOp, LLVM::GlobalOp> edge_info_global_ops;
  DenseMap<NodeOp, LLVM::GlobalOp> node_infos;
  DenseMap<EdgeOp, LLVM::GlobalOp> delay_values;
  Type i64ype;
  Type pointer_type;
  Type void_type;

  template <typename T> Type getMLIRType(MLIRContext *context) {
    return Type{};
  }

  template <> Type getMLIRType<i64>(MLIRContext *context) {
    return IntegerType::get(context, 64);
  }

  template <> Type getMLIRType<LLVM::GlobalOp>(MLIRContext *context) {
    return LLVM::LLVMPointerType::get(context);
  }

  template <typename T, typename... Args>
  void fillTypeVector(MLIRContext *context, SmallVector<Type> &vec) {
    vec.push_back(getMLIRType<T>(context));
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

  template <typename T> Value asValue(OpBuilder builder, Location loc, T t) {
    return CREATE(LLVM::ConstantOp, builder, loc,
                  asAttr(builder.getContext(), t))
        .getResult();
  }

  template <>
  Value asValue<std::nullptr_t>(OpBuilder builder, Location loc,
                                std::nullptr_t) {
    return CREATE(LLVM::ZeroOp, builder, loc,
                  LLVM::LLVMPointerType::get(builder.getContext()))
        .getResult();
  }

  template <>
  Value asValue<LLVM::GlobalOp>(OpBuilder builder, Location loc,
                                LLVM::GlobalOp global_op) {
    if (global_op)
      return CREATE(LLVM::AddressOfOp, builder, loc, global_op).getResult();
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
    auto global_builder = OpBuilder::atBlockBegin(rv.getBody());
    auto _struct = CREATE(LLVM::UndefOp, global_builder, loc, type).getResult();
    _struct = fillOutContainer(global_builder, _struct, 0, args...);
    CREATE(LLVM::ReturnOp, global_builder, loc, _struct);
    return rv;
  }

  LLVM::GlobalOp createGlobalArray(ModuleOp module, Location loc,
                                   DenseArrayAttr array, StringRef name,
                                   bool is_constant) {

    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    return CREATE(
        LLVM::GlobalOp, mod_builder, loc,
        LLVM::LLVMArrayType::get(array.getElementType(), array.getSize()),
        is_constant, LLVM::Linkage::External, name, array);
  };

  template <typename Range>
  LLVM::GlobalOp createGlobalArray(ModuleOp module, Location loc, Range &&range,
                                   StringRef name, bool is_constant) {
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto elem_type =
        getMLIRType<typename RangeUtil::OwnedElementType<Range>::type>(
            module.getContext());
    auto array_type = LLVM::LLVMArrayType::get(elem_type, llvm::size(range));
    auto rv = CREATE(LLVM::GlobalOp, mod_builder, loc, array_type, is_constant,
                     LLVM::Linkage::External, name, Attribute{});
    auto global_builder = OpBuilder::atBlockBegin(rv.getBody());
    auto array =
        CREATE(LLVM::UndefOp, global_builder, loc, array_type).getResult();
    for (auto [i, v] : llvm::enumerate(range)) {
      auto insert = CREATE(LLVM::InsertValueOp, global_builder, loc, array,
                           asValue(global_builder, loc, v), i);
    }
    CREATE(LLVM::ReturnOp, global_builder, loc, array);
    return rv;
  };

  void codegenFIFO(OpBuilder builder, EdgeOp edge) {
    auto module = edge->getParentOfType<ModuleOp>();
    auto create_call = CREATE(CallOp, builder, edge->getLoc(),
                              "iara_fifo_runtime_DynamicPushFirstFifo_create",
                              {LLVM::LLVMPointerType::get(edge->getContext())});
    this->runtime_fifo_pointers[edge] = create_call->getResult(0);

    LLVM::GlobalOp delay_global{};
    if (auto array = dyn_cast<DenseArrayAttr>(edge.delay())) {
      auto sym_name = llvm::formatv("iara_edge_{0}_delay", edge.id()).str();
      delay_global =
          createGlobalArray(module, edge->getLoc(), array, sym_name, true);
    }

    auto sym_name = llvm::formatv("iara_edge_info_{0}", edge.id()).str();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto global_op =
        createGlobalStruct<i64, i64, i64, LLVM::GlobalOp, LLVM::GlobalOp>(
            mod_builder, edge.getLoc(), sym_name, edge.prod_rate(),
            edge.cons_rate(), edge.delay_size(), delay_global,
            LLVM::GlobalOp{});

    edge_info_global_ops[edge] = global_op;

    // write pointer to fifo in global op

    auto global_address =
        CREATE(LLVM::AddressOfOp, builder, edge.getLoc(), global_op);
    auto value = CREATE(LLVM::LoadOp, builder, edge.getLoc(), global_address);
    CREATE(LLVM::InsertValueOp, builder, edge.getLoc(), value,
           create_call.getResult(0), {1});
  }

  // Node wrappers call the kernel function with prepopulated parameters, and
  // also spread out the arguments from the given array.
  FuncOp createNodeWrapper(NodeOp node) {
    auto module = node->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());
    auto sym_name = llvm::formatv("iara_node_wrapper_{0}", node.id()).str();
    SmallVector<Type> arg_types;
    for (auto i : node.getIn())
      arg_types.push_back(i.getType());

    // (getOut returns both inout and pure outs.)
    for (auto i : node.getOut())
      arg_types.push_back(i.getType());

    auto wrapper = CREATE(FuncOp, mod_builder, node.getLoc(), sym_name,
                          FunctionType::get(node.getContext(), arg_types, {}));

    wrapper.setVisibility(mlir::SymbolTable::Visibility::Public);
    wrapper->setAttr("llvm.emit_c_interface", mod_builder.getUnitAttr());
    auto func_builder = OpBuilder::atBlockBegin(wrapper.addEntryBlock());

    // kernel must have format void name(params*, in*, inout*, out*).

    SmallVector<Value> kernel_args;

    for (auto p : node.getParams()) {
      auto constant = dyn_cast<arith::ConstantOp>(p.getDefiningOp());
      assert(constant && "Params should have been uniqued by now.");
      auto new_const = func_builder.clone(*constant);
      kernel_args.push_back(new_const->getResult(0));
    }
    auto all_buffers = llvm::concat<Value>(node.getIn(), node.getOut());
    for (auto [i, v] : llvm::enumerate(all_buffers)) {
      auto pointer_pointer =
          CREATE(LLVM::GEPOp, func_builder, node.getLoc(), pointer_type,
                 func_builder.getBlock()->getArgument(0), {i});
      auto pointer =
          CREATE(LLVM::LoadOp, func_builder, node.getLoc(), pointer_pointer);
      kernel_args.push_back(pointer);
    }

    auto kernel_call = CREATE(CallOp, func_builder, node.getLoc(),
                              node.getImpl(), {}, kernel_args);

    getOrGenFuncDecl(kernel_call, true);

    CREATE(ReturnOp, func_builder, node.getLoc(), {});
  }

  LLVM::GlobalOp generateNodeInfo(NodeOp node) {
    // matches NodeInfo in IaraRuntime/DynamicPushFirstFIFO.h
    auto module = node->getParentOfType<ModuleOp>();
    auto mod_builder = OpBuilder::atBlockBegin(module.getBody());

    auto sym_name = llvm::formatv("iara_node_info_{0}", node.id()).str();

    auto input_edge_infos =
        node.getAllInputs() |
        Map{[](auto v) { return cast<EdgeOp>(v.getDefiningOp()); }} |
        Map([&](EdgeOp edge) { return edge_info_global_ops[edge]; });

    auto inputs_sym_name =
        llvm::formatv("iara_node_info_{0}_inputs", node.id()).str();
    auto input_infos_array = createGlobalArray(
        module, node.getLoc(), input_edge_infos, inputs_sym_name, true);

    auto output_edge_infos =
        node.getAllOutputs() | Map{[](auto v) -> EdgeOp {
          return cast<EdgeOp>(*v.getUser().begin());
        }} |
        Map([&](EdgeOp edge) { return edge_info_global_ops[edge]; });

    auto outputs_sym_name =
        llvm::formatv("iara_node_info_{0}_outputs", node.id()).str();
    auto output_infos_array = createGlobalArray(
        module, node.getLoc(), output_edge_infos, outputs_sym_name, true);

    auto node_wrapper = createNodeWrapper(node);

    auto node_info = createGlobalStruct<i64, i64, i64, i64, LLVM::GlobalOp,
                                        LLVM::GlobalOp, SymbolOpInterface>(
        mod_builder, node.getLoc(), sym_name, node.total_firings(),
        node.getIn().size(), node.getInout().size(), node.getPureOuts().size(),
        input_infos_array, output_infos_array, node_wrapper);

    node_infos[node] = node_info;

    return node_info;
  }

  void codegenNode(OpBuilder builder, NodeOp node) {
    auto info = generateNodeInfo(node);
    node_infos[node] = info;

    auto info_ptr = CREATE(LLVM::AddressOfOp, builder, node->getLoc(), info);

    auto firings_call = CREATE(CallOp, builder, node->getLoc(),
                               "iara_fire_node", {}, {info_ptr});
    getOrGenFuncDecl(firings_call, true);
  }

  ActorOp getMainActor();

  FuncOp convertToFunction(ActorOp actor) {
    auto main_func = mlir_util::createEmptyVoidFunctionWithBody(
        OpBuilder(actor), actor.getSymName(), actor.getLoc());

    auto main_func_builder = OpBuilder(main_func);
    main_func_builder.setInsertionPointToEnd(
        &main_func.getFunctionBody().back());

    // only once:

    auto edges = actor.getOps<EdgeOp>() | IntoVector();
    auto nodes = actor.getOps<NodeOp>() | IntoVector();

    for (auto edge : actor.getOps<EdgeOp>()) {
      codegenFIFO(main_func_builder, edge);
    }
    for (auto node : actor.getOps<NodeOp>()) {
      codegenNode(main_func_builder, node);
    }

    CREATE(ReturnOp, main_func_builder, main_func.getLoc(), {});

    actor->erase();

    return main_func;
  }

  void runOnOperation(ModuleOp module) {
    auto main_actor = getMainActor();
    i64ype = OpBuilder(module).getI64Type();
    pointer_type = LLVM::LLVMPointerType::get(module->getContext());
    void_type = LLVM::LLVMVoidType::get(module.getContext());
    convertToFunction(main_actor);
  }
};

void DynamicPushFirstFIFOSchedulerPass::runOnOperation() {
  pimpl = new Impl();
  pimpl->runOnOperation(getOperation());
}

} // namespace mlir::iara::passes::fifo
