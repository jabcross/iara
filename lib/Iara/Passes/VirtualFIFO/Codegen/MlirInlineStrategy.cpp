#include "Iara/Dialect/IaraOps.h"
#include "Iara/Dialect/Node.h"
#include "Iara/Passes/Common/Codegen/AsValue.h"
#include "Iara/Passes/Common/Codegen/Codegen.h"
#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Internal.h"
#include "Iara/Passes/VirtualFIFO/Codegen/StaticDataEmitStrategy.h"
#include "Iara/Util/ForEachType.h"
#include "Iara/Util/Mlir.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FormatVariadic.h>
#include <memory>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <span>

namespace iara::passes::virtualfifo::codegen {

using namespace iara::util::mlir;
using namespace mlir;
using namespace mlir::LLVM;
using namespace iara::dialect;
using namespace iara::passes::common::codegen;

namespace {

struct MlirInlineEmitter {
  ModuleOp module;
  OpBuilder module_builder;
  std::span<NodeCodegenData> node_pairs;
  std::span<EdgeCodegenData> edge_codegen_datas;
  MLIRContext *ctx;

  Type i64type;
  Type opaque_ptr;

  MlirInlineEmitter(ModuleOp module,
                    OpBuilder module_builder,
                    std::span<NodeCodegenData> nodes,
                    std::span<EdgeCodegenData> edges)
      : module(module), module_builder(module_builder), node_pairs(nodes),
        edge_codegen_datas(edges), ctx(module->getContext()) {
    i64type = module_builder.getI64Type();
    opaque_ptr = LLVMPointerType::get(ctx);
  }

  GlobalOp
  makeGlobalStruct(OpBuilder builder,
                   Location loc,
                   StringRef name,
                   Type type,
                   bool is_constant,
                   std::function<Value(OpBuilder, Location)> inserter) {
    auto rv = CREATE(GlobalOp,
                     builder,
                     loc,
                     type,
                     is_constant,
                     Linkage::External,
                     name,
                     {},
                     DataLayout::closest(module).getTypeABIAlignment(type));
    rv.getInitializer().emplaceBlock();
    auto init_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
    auto val = inserter(init_builder, loc);
    CREATE(ReturnOp, init_builder, loc, val);
    return rv;
  }

  std::pair<std::function<void(Value, Location)>, OpBuilder>
  makeGlobalArray(
      StringRef name, Type elem_type, i64 align, Location loc, i64 size) {
    OpBuilder builder = OpBuilder::atBlockBegin(module.getBody());
    auto array_type = LLVMArrayType::get(elem_type, size);
    auto global_op = CREATE(GlobalOp,
                            builder,
                            loc,
                            array_type,
                            false,
                            Linkage::External,
                            name,
                            {},
                            align);
    global_op.getInitializer().emplaceBlock();
    auto global_init_builder =
        OpBuilder::atBlockBegin(global_op.getInitializerBlock());

    UndefOp undef_op = CREATE(UndefOp, global_init_builder, loc, array_type);

    Value wip_value = undef_op.getResult();

    auto return_op = CREATE(ReturnOp, global_init_builder, loc, wip_value);

    OpBuilder values_builder(return_op);

    auto inserter = [=, index = (i64)0](Value new_value, Location loc) mutable {
      assert(new_value.getType() == elem_type);
      auto insert_op = CREATE(
          InsertValueOp, values_builder, loc, wip_value, new_value, index++);
      Value new_struct = insert_op.getResult();
      wip_value.replaceAllUsesExcept(new_struct, insert_op);
      wip_value = new_struct;
      assert(global_op.verify().succeeded());
    };

    return {inserter, values_builder};
  }

  Value
  buildWrapper(OpBuilder builder, Location loc, NodeCodegenData node_pair) {
    return CREATE(
        AddressOfOp, builder, loc, opaque_ptr, node_pair.wrapper.getSymName());
  }

  Value makeOffsetPointer(OpBuilder builder,
                          Location loc,
                          Type elem_type,
                          StringRef name,
                          i64 index) {
    Value first = CREATE(AddressOfOp, builder, loc, opaque_ptr, name);

    llvm::errs() << getTypeSize(elem_type, DataLayout::closest(module));
    Value gep =
        CREATE(GEPOp, builder, loc, opaque_ptr, elem_type, first, {(i32)index});
    return gep;
  }

  Value makeSpan(OpBuilder builder,
                 Location loc,
                 Type elem_type,
                 StringRef name,
                 i64 size) {
    auto span_type = getSpanType(ctx);
    Value _struct = CREATE(UndefOp, builder, loc, span_type);
    auto ptr = makeOffsetPointer(builder, loc, elem_type, name, 0);
    _struct =
        CREATE(InsertValueOp, builder, loc, _struct, ptr, ArrayRef<i64>{0});
    _struct = CREATE(InsertValueOp,
                     builder,
                     loc,
                     _struct,
                     createConstOp(builder, loc, size),
                     ArrayRef<i64>{1});
    return _struct;
  }

  std::string getEdgeDelayName(i64 id, EdgeOp edge_op) {
    // Use operation pointer for guaranteed uniqueness during codegen
    return (std::string)llvm::formatv("iara_runtime_data_delay_{0}_{1}", id, (void*)edge_op.getOperation());
  }

  Value getDelayData(OpBuilder builder,
                     Location loc,
                     EdgeCodegenData &edge_codegen_data) {
    Edge e(edge_codegen_data.edge_op);
    if (e.delaySize() == 0)
      return getEmptySpan(builder, loc);
    return makeSpan(builder,
                    loc,
                    builder.getI8Type(),
                    getEdgeDelayName(edge_codegen_data.index, edge_codegen_data.edge_op),
                    e.delaySize());
  }

  Value makeNodeInfo(OpBuilder builder,
                     NodeCodegenData &node_codegen_data,
                     Location loc) {

    Node n(node_codegen_data.node_op);
    VirtualFIFO_Node_StaticInfo si{};
    si.id = node_codegen_data.index;
    si.arg_bytes = n.argBytes();
    si.num_args = n.numArgs();
    si.rank = n.rank();
    si.total_iter_firings = n.totalIterFirings();
    si.needs_priming = n.needsPriming();
    auto static_info_val = asValue(builder, loc, si);

    auto input_fifos_placeholder = getEmptySpan(builder, loc);
    auto output_fifos_placeholder = getEmptySpan(builder, loc);

    node_codegen_data.input_fifos_span_ptr = input_fifos_placeholder;
    node_codegen_data.output_fifos_span_ptr = output_fifos_placeholder;

    auto node = node_codegen_data.node_op;

    node_codegen_data.name = getDebugName(node);

    auto name_global =
        LLVM::createGlobalString(node->getLoc(),
                                 builder,
                                 node_codegen_data.name,
                                 node_codegen_data.name,
                                 LLVM::linkage::Linkage::External);

    auto codegen_info_val = createIdentifiedLLVMStructValue(
        builder,
        loc,
        VirtualFIFO_Node_CodegenInfo::STRUCT_NAME,
        {
            {opaque_ptr, name_global},
            {opaque_ptr, buildWrapper(builder, loc, node_codegen_data)},
            {getSpanType(ctx), input_fifos_placeholder},
            {getSpanType(ctx), output_fifos_placeholder},
        });

    auto runtime_info_val = createIdentifiedLLVMStructValue(
        builder,
        loc,
        VirtualFIFO_Node_RuntimeInfo::STRUCT_NAME,
        {{opaque_ptr, getNullPtr(builder, loc)}});

    auto node_info_val = createIdentifiedLLVMStructValue(
        builder,
        loc,
        VirtualFIFO_Node::STRUCT_NAME,
        {{static_info_val.getType(), static_info_val},
         {codegen_info_val.getType(), codegen_info_val},
         {runtime_info_val.getType(), runtime_info_val}});

    return node_info_val;
  }

  Value makeEdgeInfo(OpBuilder builder,
                     EdgeCodegenData &edge_codegen_data,
                     Location loc) {

    auto edge_op = edge_codegen_data.edge_op;

    Edge e(edge_op);
    VirtualFIFO_Edge_StaticInfo si{};
    si.id = edge_codegen_data.index;
    si.local_index = e.localIndex();
    si.prod_rate = e.prodRate();
    si.cons_rate = e.consRate();
    si.cons_arg_idx = e.consArgIdx();
    si.delay_offset = e.delayOffset();
    si.delay_size = e.delaySize();
    si.block_size_with_delays = e.blockSizeWithDelays();
    si.block_size_no_delays = e.blockSizeNoDelays();
    si.prod_alpha = e.prodAlpha();
    si.prod_beta = e.prodBeta();
    si.cons_alpha = e.consAlpha();
    si.cons_beta = e.consBeta();
    auto static_info_val = asValue(builder, edge_op->getLoc(), si);

    auto producer_ptr = getNullPtr(builder, loc);
    auto consumer_ptr = getNullPtr(builder, loc);
    auto alloc_node_ptr = getNullPtr(builder, loc);
    auto next_in_chain_ptr = getNullPtr(builder, loc);

    edge_codegen_data.producer_node_ptr = producer_ptr;
    edge_codegen_data.consumer_node_ptr = consumer_ptr;
    edge_codegen_data.alloc_node_ptr = alloc_node_ptr;
    edge_codegen_data.next_edge_ptr = next_in_chain_ptr;

    auto edge_name = getDebugName(edge_op);

    auto name_global =
        LLVM::createGlobalString(edge_op->getLoc(),
                                 builder,
                                 edge_name,
                                 edge_name,
                                 LLVM::linkage::Linkage::External);

    Value codegen_info_val = createIdentifiedLLVMStructValue(
        builder,
        loc,
        VirtualFIFO_Edge_CodegenInfo::STRUCT_NAME,
        {{opaque_ptr, name_global},
         {getSpanType(ctx), getDelayData(builder, loc, edge_codegen_data)},
         {opaque_ptr, consumer_ptr},
         {opaque_ptr, producer_ptr},
         {opaque_ptr, alloc_node_ptr},
         {opaque_ptr, next_in_chain_ptr}});

    Value edge_info_val = createIdentifiedLLVMStructValue(
        builder,
        loc,
        VirtualFIFO_Edge::STRUCT_NAME,
        {{getMLIRType<VirtualFIFO_Edge_StaticInfo>(ctx), static_info_val},
         {getMLIRType<VirtualFIFO_Edge_CodegenInfo>(ctx), codegen_info_val}});

    return edge_info_val;
  }

  void emit() {
    // delays
    for (auto [i, edge_codegen_data] : llvm::enumerate(edge_codegen_datas)) {
      using namespace iara::util::foreachtype;
      auto edge = edge_codegen_data.edge_op;
      DenseArrayAttr delay_attr = nullptr;

      if (auto int_attr =
              llvm::dyn_cast_or_null<IntegerAttr>(edge["delay"].get())) {
        llvm_unreachable("Should be already raised to delay attr");
      }

      if (auto dense_attr =
              llvm::dyn_cast_or_null<DenseArrayAttr>(edge["delay"].get())) {
        delay_attr = dense_attr;
      }

      if (!delay_attr) {
        continue;
      }

      auto elem_type = getElementTypeOrSelf(delay_attr.getElementType());
      bool found = false;

      for_each_type<DenseI8ArrayAttr,
                    DenseI16ArrayAttr,
                    DenseI32ArrayAttr,
                    DenseI64ArrayAttr,
                    DenseF32ArrayAttr,
                    DenseF64ArrayAttr>([&]<class T, size_t i>(TypeWrapper<T>,
                                                              Index<i>) {
        auto attr = dyn_cast<T>(delay_attr);
        if (!attr)
          return;
        assert(found == false);
        found = true;
        auto span = attr.asArrayRef();

        auto dense = DenseElementsAttr::get(
            RankedTensorType::get({delay_attr.getSize()}, elem_type), span);

        CREATE(GlobalOp,
               module_builder,
               edge.getLoc(),
               LLVMArrayType::get(attr.getElementType(), span.size()),
               false,
               Linkage::External,
               getEdgeDelayName(edge_codegen_data.index, edge),
               dense,
               DataLayout::closest(edge).getTypeABIAlignment(elem_type));
      });
      assert(found);
    }

    auto node_struct_type = getMLIRType<VirtualFIFO_Node>(ctx);
    auto edge_struct_type = getMLIRType<VirtualFIFO_Edge>(ctx);

    // node infos
    {
      auto [inserter, builder] =
          makeGlobalArray("iara_runtime_data__node_infos",
                          getMLIRType<VirtualFIFO_Node>(ctx),
                          alignof(VirtualFIFO_Node),
                          module.getLoc(),
                          node_pairs.size());

      for (auto &pair : node_pairs) {
        auto nodeinfo = makeNodeInfo(builder, pair, pair.node_op->getLoc());
        inserter(nodeinfo, pair.node_op.getLoc());
      }
    }
    // edge infos
    {
      auto [inserter, builder] =
          makeGlobalArray("iara_runtime_data__edge_infos",
                          getMLIRType<VirtualFIFO_Edge>(ctx),
                          alignof(VirtualFIFO_Edge),
                          module.getLoc(),
                          edge_codegen_datas.size());

      for (auto &pair : edge_codegen_datas) {
        auto info = makeEdgeInfo(builder, pair, pair.edge_op->getLoc());
        inserter(info, pair.edge_op->getLoc());
      }
    }

    // node input/output fifo lists
    for (auto [i, node_codegen_data] : llvm::enumerate(node_pairs)) {

      auto node_op = node_codegen_data.node_op;

      auto stitch_span = [&](std::vector<EdgeCodegenData *> &list,
                             const char *suffix,
                             Value old) {
        auto [inserter, builder] = makeGlobalArray(
            (std::string)llvm::formatv("iara_runtime_data_node_{0}_{1}",
                                       node_codegen_data.index,
                                       suffix),
            opaque_ptr,
            alignof(VirtualFIFO_Edge),
            module.getLoc(),
            list.size());

        for (auto &edge_codegen_data : list) {
          auto edge_op = edge_codegen_data->edge_op;
          auto node_loc = node_op.getLoc();
          auto edge_loc = edge_op.getLoc();
          inserter(makeOffsetPointer(builder,
                                     node_loc,
                                     edge_struct_type,
                                     "iara_runtime_data__edge_infos",
                                     edge_codegen_data->index),
                   edge_loc);
        }

        auto node_struct_builder = OpBuilder(old.getDefiningOp());
        auto loc = node_op.getLoc();
        Value new_span = makeSpan(
            node_struct_builder,
            loc,
            opaque_ptr,
            (std::string)llvm::formatv("iara_runtime_data_node_{0}_{1}",
                                       node_codegen_data.index,
                                       suffix),
            list.size());
        old.replaceAllUsesWith(new_span);
        old.getDefiningOp()->erase();
        assert(module.verify().succeeded());
      };

      stitch_span(node_codegen_data.inputs,
                  "input_fifos",
                  node_codegen_data.input_fifos_span_ptr);
      stitch_span(node_codegen_data.outputs,
                  "output_fifos",
                  node_codegen_data.output_fifos_span_ptr);
    }

    // edge consumers, producers, alloc nodes and next pointers
    for (auto &edge_codegen_data : edge_codegen_datas) {

      auto replace_with_element_of_global_array =
          [&](i64 index, StringRef array_name, Type elem_type, Value *old) {
            auto builder = OpBuilder(old->getDefiningOp());
            auto loc = old->getLoc();
            auto _new =
                makeOffsetPointer(builder, loc, elem_type, array_name, index);
            old->replaceAllUsesWith(_new);
            old->getDefiningOp()->erase();
            *old = _new;
          };

      replace_with_element_of_global_array(
          edge_codegen_data.producer->index,
          "iara_runtime_data__node_infos",
          node_struct_type,
          &edge_codegen_data.producer_node_ptr);
      replace_with_element_of_global_array(
          edge_codegen_data.consumer->index,
          "iara_runtime_data__node_infos",
          node_struct_type,
          &edge_codegen_data.consumer_node_ptr);
      replace_with_element_of_global_array(edge_codegen_data.alloc_node->index,
                                           "iara_runtime_data__node_infos",
                                           node_struct_type,
                                           &edge_codegen_data.alloc_node_ptr);
      if (edge_codegen_data.next_edge != nullptr)
        replace_with_element_of_global_array(edge_codegen_data.next_edge->index,
                                             "iara_runtime_data__edge_infos",
                                             edge_struct_type,
                                             &edge_codegen_data.next_edge_ptr);
    }

    // make global spans for debugging
    makeGlobalStruct(module_builder,
                     module.getLoc(),
                     "iara_runtime_nodes",
                     getSpanType(ctx),
                     true,
                     [&](OpBuilder builder, Location loc) -> Value {
                       return makeSpan(builder,
                                       loc,
                                       node_struct_type,
                                       "iara_runtime_data__node_infos",
                                       node_pairs.size());
                     });

    makeGlobalStruct(module_builder,
                     module.getLoc(),
                     "iara_runtime_edges",
                     getSpanType(ctx),
                     true,
                     [&](OpBuilder builder, Location loc) -> Value {
                       return makeSpan(builder,
                                       loc,
                                       edge_struct_type,
                                       "iara_runtime_data__edge_infos",
                                       edge_codegen_datas.size());
                     });
  }
};

struct MlirInlineStrategy : public StaticDataEmitStrategy {
  void emit(ModuleOp module,
            OpBuilder module_builder,
            std::span<NodeCodegenData> nodes,
            std::span<EdgeCodegenData> edges) override {
    MlirInlineEmitter emitter(module, module_builder, nodes, edges);
    emitter.emit();
  }
};

} // namespace

std::unique_ptr<StaticDataEmitStrategy> makeMlirInlineStrategy() {
  return std::make_unique<MlirInlineStrategy>();
}

} // namespace iara::passes::virtualfifo::codegen
