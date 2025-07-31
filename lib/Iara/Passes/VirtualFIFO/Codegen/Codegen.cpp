#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/Common/Codegen/GetMLIRType.h"
#include "Iara/Passes/VirtualFIFO/Codegen/Codegen.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/ForEachType.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Range.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Edge.h"
#include "IaraRuntime/virtual-fifo/VirtualFIFO_Node.h"
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/algorithm.hpp>
#include <cassert>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Types.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Support/LLVM.h>
#include <span>

namespace iara::passes::common::codegen {

// template <> struct GetMLIRType<VirtualFIFO_Node::StaticInfo> {
//   static Type get(MLIRContext *context) {
//     Vec<Type> node_info_struct_field_types{
//         VirtualFIFO_Node::static_info_num_fields,
//         mlir::IntegerType::get(context, 64)};
//     return LLVM::LLVMStructType::getLiteral(context,
//                                             node_info_struct_field_types);
//   }
// };

// template <> struct GetMLIRType<VirtualFIFO_Node::CodegenInfo> {
//   static Type get(MLIRContext *context) {
//     Vec<Type> node_info_struct_field_types{
//         VirtualFIFO_Node::codegen_info_num_fields,
//         mlir::IntegerType::get(context, 64)};
//     return LLVM::LLVMStructType::getLiteral(context,
//                                             node_info_struct_field_types);
//   }
// };

// template <> struct GetMLIRType<VirtualFIFO_Node> {
//   static Type get(MLIRContext *context) {
//     auto opaque_ptr = LLVM::LLVMPointerType::get(context);
//     auto span_type = LLVM::LLVMStructType::getLiteral(
//         context, {opaque_ptr, IntegerType::get(context, 64)});
//     return LLVM::LLVMStructType::getLiteral(
//         context,
//         {
//             opaque_ptr,                                         // name
//             getMLIRType<VirtualFIFO_Node::StaticInfo>(context), // info
//             opaque_ptr,                                         // wrapper
//             span_type,                                          //
//             input_fifos span_type, // output_fifos opaque_ptr, // semaphore
//         });
//   }
// };

template <class T> struct AsValue {};

template <class T>
concept IsDescribed = requires {
  boost::describe::describe_members<T, boost::describe::mod_public>{};
};

template <class T>
  requires IsDescribed<T>
struct AsValue<T> {
  static Value get(OpBuilder builder, Location loc, T &_struct) {
    using namespace iara::util::foreachtype;
    static_assert(sizeof(T) == sizeof(typename T::TupleType));
    DEF_OP(Value,
           struct_value,
           LLVM::UndefOp,
           builder,
           loc,
           getMLIRType<T>(builder.getContext()));
    Vec<Value> members;

    size_t counter = 0;

    boost::mp11::mp_for_each<
        boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto member_ptr) {
          auto val = _struct.*member_ptr.pointer;
          auto value = AsValue<decltype(val)>::get(builder, loc, val);
          members.push_back(value);
          struct_value = CREATE(LLVM::InsertValueOp,
                                builder,
                                loc,
                                struct_value,
                                value,
                                counter++);
        });

    return struct_value;
  }
};

Value createConstOp(OpBuilder builder, Location loc, auto val) {
  DEF_OP(Value,
         _const,
         arith::ConstantOp,
         builder,
         loc,
         asAttr(builder.getContext(), val));
  return _const;
}

template <class T>
concept AsConstOp = requires(OpBuilder builder, Location loc, T t) {
  asAttr(builder.getContext(), t);
  createConstOp(builder, loc, t);
};

template <class T>
  requires AsConstOp<T>
struct AsValue<T> {
  static Value get(OpBuilder builder, Location loc, auto val) {
    return createConstOp(builder, loc, val);
  }
};

template <class T> Value asValue(OpBuilder builder, Location loc, T value) {
  return AsValue<T>::get(builder, loc, value);
}

LLVM::LLVMStructType getLiteralLLVMStructType(MLIRContext *ctx,
                                              Vec<Type> types) {
  return LLVM::LLVMStructType::getLiteral(ctx, types);
}

Value createLLVMStruct(OpBuilder builder,
                       Location loc,
                       Vec<std::pair<Type, Value>> pairs) {

  auto struct_type = getLiteralLLVMStructType(
      builder.getContext(), pairs | util::range::Map([](auto pair) {
                              return pair.first;
                            }) | util::range::IntoVector());

  DEF_OP(Value, struct_value, LLVM::UndefOp, builder, loc, struct_type);

  for (auto [i, pair] : enumerate(pairs)) {
    auto [type, value] = pair;
    struct_value =
        CREATE(LLVM::InsertValueOp, builder, loc, struct_value, value, i);
  }
  return struct_value;
}

// using namespace iara::passes::virtualfifo::codegen;

// Vec<i64> values = {info.id,
//                    info.arg_bytes,
//                    info.num_args,
//                    info.rank,
//                    info.total_iter_firings,
//                    info.needs_priming};

// assert(values.size() == VirtualFIFO_Node::static_info_num_fields);
// Value static_info =
//     CREATE(LLVM::UndefOp,
//            builder,
//            loc,
//            getMLIRType<VirtualFIFO_Node::StaticInfo>(builder.getContext()));

// i64 index = 0;
// for (auto v : values) {
//   static_info = CREATE(LLVM::InsertValueOp,
//                        builder,
//                        loc,
//                        static_info,
//                        createConstOp(builder, loc, v),
//                        index++);
// }
// return static_info;
// }

// template <> struct GetMLIRType<VirtualFIFO_Edge::StaticInfo> {
//   static Type get(MLIRContext *context) {
//     Vec<Type> edge_info_struct_field_types{
//         VirtualFIFO_Edge::static_info_num_fields,
//         mlir::IntegerType::get(context, 64)};
//     return LLVM::LLVMStructType::getLiteral(context,
//                                             edge_info_struct_field_types);
//   }
// };

// template <> struct GetMLIRType<VirtualFIFO_Edge> {
//   static Type get(MLIRContext *context) {
//     auto opaque_ptr = LLVM::LLVMPointerType::get(context);
//     return LLVM::LLVMStructType::getLiteral(
//         context,
//         {
//             opaque_ptr,                                         // name
//             getMLIRType<VirtualFIFO_Edge::StaticInfo>(context), // info
//             getSpanType(context),                               // delay_data
//             opaque_ptr,                                         // consumer
//             opaque_ptr,                                         // producer
//             opaque_ptr,                                         // alloc_node
//             opaque_ptr                                          //
//             next_in_chain
//         });
//   }
// };

} // namespace iara::passes::common::codegen

namespace iara::passes::virtualfifo::codegen {
using namespace iara::passes::virtualfifo::sdf;
using namespace iara::util::mlir;
using namespace mlir::LLVM;
using namespace iara::dialect;
using namespace iara::passes::common::codegen;

void fillOutPairPointers(std::span<NodeCodegenData> node_codegen_datas,
                         std::span<EdgeCodegenData> edge_codegen_datas) {
  DenseMap<NodeOp, NodeCodegenData *> node_to_data;
  DenseMap<EdgeOp, EdgeCodegenData *> edge_to_data;

  for (auto &node_codegen_data : node_codegen_datas) {
    node_to_data[node_codegen_data.node_op] = &node_codegen_data;
  }
  for (auto &edge_codegen_data : edge_codegen_datas) {
    edge_to_data[edge_codegen_data.edge_op] = &edge_codegen_data;
  }

  for (auto &node_codegen_data : node_codegen_datas) {
    for (auto input : node_codegen_data.node_op.getAllInputs()) {
      auto edge_op = llvm::cast<EdgeOp>(input.getDefiningOp());
      assert(edge_to_data.contains(edge_op));
      auto edge_codegen_data = edge_to_data[edge_op];
      node_codegen_data.inputs.push_back(edge_codegen_data);
      edge_codegen_data->consumer = &node_codegen_data;
    }
    for (auto output : node_codegen_data.node_op.getAllOutputs()) {
      auto users = llvm::to_vector(output.getUsers());
      assert(users.size() == 1);
      auto edge_op = llvm::cast<EdgeOp>(users.front());
      assert(edge_to_data.contains(edge_op));
      auto edge_codegen_data = edge_to_data[edge_op];
      node_codegen_data.outputs.push_back(edge_codegen_data);
      edge_codegen_data->producer = &node_codegen_data;
    }
  }
  for (auto &edge_codegen_data : edge_codegen_datas) {
    edge_codegen_data.alloc_node =
        node_to_data[findFirstNodeOfChain(edge_codegen_data.edge_op)];
    assert(edge_codegen_data.alloc_node->node_op.isAlloc());

    auto next_edge_op = followInoutChainForwards(edge_codegen_data.edge_op);
    if (next_edge_op) {
      auto data = edge_to_data[next_edge_op];
      edge_codegen_data.next_edge = data;
    }
  }
}

// helper functions

std::string getDebugName(NodeOp node) {
  if (node.isAlloc()) {
    auto alloc_edge = cast<EdgeOp>(*node->getResult(0).getUsers().begin());
    auto first_node = getConsumerNode(alloc_edge);
    auto index = alloc_edge->getUses().begin()->getOperandNumber();
    return llvm::formatv("allocNode_{3}_{2}_{0}[{1}]\0",
                         first_node.getImpl(),
                         index,
                         (i64)first_node["id"],
                         (i64)node["id"]);
  }
  if (node.isDealloc()) {
    auto dealloc_edge = cast<EdgeOp>(node->getOperand(0).getDefiningOp());
    auto last_node = getProducerNode(dealloc_edge);
    auto index = util::mlir::getResultIndex(dealloc_edge.getIn());
    return llvm::formatv("deallocNode_{3}_{2}_{0}[{1}]\0",
                         last_node.getImpl(),
                         index,
                         (i64)last_node["id"],
                         (i64)node["id"]);
  }
  return llvm::formatv("node_{0}_{1}\0", (i64)node["id"], node.getImpl());
}

std::string getDebugName(EdgeOp edge) {
  auto prod = getProducerNode(edge);
  auto cons = getConsumerNode(edge);
  auto prod_index = util::mlir::getResultIndex(edge.getIn());
  auto cons_index = edge->getUses().begin()->getOperandNumber();
  if (prod.isAlloc()) {
    return llvm::formatv("allocEdge_{3}_{0}_{1}[{2}]\0",
                         (i64)cons["id"],
                         cons.getImpl(),
                         cons_index,
                         (i64)edge["id"]);
  }
  if (cons.isDealloc()) {
    return llvm::formatv("deallocNode_{3}_{0}_{1}[{2}]\0",
                         (i64)prod["id"],
                         prod.getImpl(),
                         prod_index,
                         (i64)edge["id"]);
  }
  return llvm::formatv("edge_{6}_{4}_{0}[{1}]->{5}_{2}[{3}]\0",
                       prod.getImpl(),
                       prod_index,
                       cons.getImpl(),
                       cons_index,
                       (i64)prod["id"],
                       (i64)cons["id"],
                       (i64)edge["id"]);
}

Value createConstOp(OpBuilder builder, Location loc, auto val) {
  DEF_OP(Value,
         _const,
         ConstantOp,
         builder,
         loc,
         getMLIRType<decltype(val)>(builder.getContext()),
         val);
  return _const;
}

Value getNullPtr(OpBuilder builder, Location loc) {
  return CREATE(
      ZeroOp, builder, loc, LLVM::LLVMPointerType::get(builder.getContext()));
}

Value getEmptySpan(OpBuilder builder, Location loc) {
  Value _struct =
      CREATE(UndefOp, builder, loc, getSpanType(builder.getContext()));
  _struct = CREATE(InsertValueOp,
                   builder,
                   loc,
                   _struct,
                   getNullPtr(builder, loc),
                   ArrayRef<i64>{0});
  _struct = CREATE(InsertValueOp,
                   builder,
                   loc,
                   _struct,
                   createConstOp(builder, loc, i64(0)),
                   ArrayRef<i64>{1});
  return _struct;
}

// struct codegen

// Value asValue(OpBuilder builder,
//               Location loc,
//               VirtualFIFO_Node::StaticInfo &info) {

//   using namespace iara::passes::virtualfifo::codegen;

//   Vec<i64> values = {info.id,
//                      info.arg_bytes,
//                      info.num_args,
//                      info.rank,
//                      info.total_iter_firings,
//                      info.needs_priming};

//   assert(values.size() == VirtualFIFO_Node::static_info_num_fields);
//   Value static_info =
//       CREATE(LLVM::UndefOp,
//              builder,
//              loc,
//              getMLIRType<VirtualFIFO_Node::StaticInfo>(builder.getContext()));

//   i64 index = 0;
//   for (auto v : values) {
//     static_info = CREATE(LLVM::InsertValueOp,
//                          builder,
//                          loc,
//                          static_info,
//                          createConstOp(builder, loc, v),
//                          index++);
//   }
//   return static_info;
// }

struct CodegenStaticData::Impl {

  ModuleOp module;
  std::span<NodeCodegenData> node_pairs;
  std::span<EdgeCodegenData> edge_codegen_datas;
  MLIRContext *ctx;
  OpBuilder module_builder;

  Type i64type;
  Type opaque_ptr;

  Impl(ModuleOp module,
       OpBuilder module_builder,
       std::span<NodeCodegenData> node_pairs,
       std::span<EdgeCodegenData> edge_pairs)
      : module(module), node_pairs(node_pairs), edge_codegen_datas(edge_pairs),
        ctx(module->getContext()), module_builder(module_builder) {

    fillOutPairPointers(node_pairs, edge_pairs);
    i64type = module_builder.getI64Type();
    opaque_ptr = LLVMPointerType::get(ctx);
  };

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

  std::pair<std::function<void(Value, Location)>, std::shared_ptr<OpBuilder>>
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

    std::shared_ptr<OpBuilder> values_builder =
        std::make_shared<OpBuilder>(return_op);

    auto lambda = [=, index = (i64)0](Value new_value, Location loc) mutable {
      assert(new_value.getType() == elem_type);
      auto insert_op = CREATE(
          InsertValueOp, *values_builder, loc, wip_value, new_value, index++);
      Value new_struct = insert_op.getResult();
      wip_value.replaceAllUsesExcept(new_struct, insert_op);
      wip_value = new_struct;
      assert(global_op.verify().succeeded());
    };

    return {lambda, values_builder};
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

  Type getEdgeElementType(EdgeOp edge) {
    return getElementTypeOrSelf(edge.getIn().getType());
  }

  std::string getEdgeDelayName(i64 id) {
    return (std::string)llvm::formatv("iara_runtime_data_delay_{0}", id);
  }

  Value getDelayData(OpBuilder builder,
                     Location loc,
                     EdgeCodegenData &edge_codegen_data) {
    auto &static_info = edge_codegen_data.static_info;
    if (static_info.delay_size == 0)
      return getEmptySpan(builder, loc);
    return makeSpan(builder,
                    loc,
                    builder.getI8Type(),
                    getEdgeDelayName(static_info.id),
                    edge_codegen_data.static_info.delay_size);
  }

  Value makeNodeInfo(OpBuilder builder,
                     NodeCodegenData &node_codegen_data,
                     Location loc) {

    using namespace iara::passes::virtualfifo::codegen;

    auto static_info_val = asValue(builder, loc, node_codegen_data.static_info);

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

    auto codegen_info_val = createLLVMStruct(
        builder,
        loc,
        {
            {opaque_ptr, name_global},
            {opaque_ptr, buildWrapper(builder, loc, node_codegen_data)},
            {getSpanType(ctx), input_fifos_placeholder},
            {getSpanType(ctx), output_fifos_placeholder},
        });

    auto runtime_info_val = createLLVMStruct(
        builder, loc, {{opaque_ptr, getNullPtr(builder, loc)}});

    auto node_info_val =
        createLLVMStruct(builder,
                         loc,
                         {{static_info_val.getType(), static_info_val},
                          {codegen_info_val.getType(), codegen_info_val},
                          {runtime_info_val.getType(), runtime_info_val}});

    return node_info_val;
  }

  Value makeEdgeInfo(OpBuilder builder,
                     EdgeCodegenData &edge_codegen_data,
                     Location loc) {

    auto edge_op = edge_codegen_data.edge_op;
    // auto &edge_info = edge_pair.edge_info;

    auto static_info_val =
        asValue(builder, edge_op->getLoc(), edge_codegen_data.static_info);

    // Vec<i64> static_info_data = {
    //     edge_info.static_info.id,
    //     edge_info.static_info.local_index,
    //     edge_info.static_info.prod_rate,
    //     edge_info.static_info.cons_rate,
    //     edge_info.static_info.cons_arg_idx,
    //     edge_info.static_info.delay_offset,
    //     edge_info.static_info.delay_size,
    //     edge_info.static_info.block_size_with_delays,
    //     edge_info.static_info.block_size_no_delays,
    //     edge_info.static_info.prod_alpha,
    //     edge_info.static_info.prod_beta,
    //     edge_info.static_info.cons_alpha,
    //     edge_info.static_info.cons_beta,
    // };

    // Value static_info = CREATE(
    //     UndefOp, builder, loc,
    //     getMLIRType<VirtualFIFO_Edge::StaticInfo>(ctx));

    // {
    //   i64 index = 0;
    //   for (auto v : static_info_data) {
    //     static_info = CREATE(InsertValueOp,
    //                          builder,
    //                          loc,
    //                          static_info,
    //                          createConstOp(builder, loc, v),
    //                          index++);
    //   }
    // }

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

    Value codegen_info_val = createLLVMStruct(
        builder,
        loc,
        {{opaque_ptr, name_global},
         {getSpanType(ctx), getDelayData(builder, loc, edge_codegen_data)},
         {opaque_ptr, consumer_ptr},
         {opaque_ptr, producer_ptr},
         {opaque_ptr, alloc_node_ptr},
         {opaque_ptr, next_in_chain_ptr}});

    Value edge_info_val = createLLVMStruct(
        builder,
        loc,
        {{getMLIRType<VirtualFIFO_Edge::StaticInfo>(ctx), static_info_val},
         {getMLIRType<VirtualFIFO_Edge::CodegenInfo>(ctx), codegen_info_val}});

    return edge_info_val;
  };

  // std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  void codegenStaticData() {
    // delays
    for (auto [i, edge_codegen_data] : llvm::enumerate(edge_codegen_datas)) {
      {
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
          // convert to dense elements, which is what LLVM wants
          auto span = attr.asArrayRef();

          auto dense = DenseElementsAttr::get(
              RankedTensorType::get({delay_attr.getSize()}, elem_type), span);

          CREATE(GlobalOp,
                 module_builder,
                 edge.getLoc(),
                 LLVMArrayType::get(attr.getElementType(), span.size()),
                 false,
                 Linkage::External,
                 getEdgeDelayName(edge_codegen_data.static_info.id),
                 dense,
                 DataLayout::closest(edge).getTypeABIAlignment(elem_type));
        });
        assert(found);
      }
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
        auto nodeinfo = makeNodeInfo(*builder, pair, pair.node_op->getLoc());
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
        inserter(makeEdgeInfo(*builder, pair, pair.edge_op->getLoc()),
                 pair.edge_op->getLoc());
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
                                       node_codegen_data.static_info.id,
                                       suffix),
            opaque_ptr,
            alignof(VirtualFIFO_Edge),
            module.getLoc(),
            list.size());

        for (auto &edge_codegen_data : list) {
          // auto &edge_info = edge_pair->edge_info;
          auto edge_op = edge_codegen_data->edge_op;
          auto node_loc = node_op.getLoc();
          auto edge_loc = edge_op.getLoc();
          inserter(makeOffsetPointer(*builder,
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
                                       node_codegen_data.static_info.id,
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
          [&](i64 index, StringRef array_name, Value *old) {
            auto builder = OpBuilder(old->getDefiningOp());
            auto loc = old->getLoc();
            auto _new = makeOffsetPointer(
                builder, loc, node_struct_type, array_name, index);
            old->replaceAllUsesWith(_new);
            old->getDefiningOp()->erase();
            *old = _new;
          };

      replace_with_element_of_global_array(
          edge_codegen_data.producer->index,
          "iara_runtime_data__node_infos",
          &edge_codegen_data.producer_node_ptr);
      replace_with_element_of_global_array(
          edge_codegen_data.consumer->index,
          "iara_runtime_data__node_infos",
          &edge_codegen_data.consumer_node_ptr);
      replace_with_element_of_global_array(edge_codegen_data.alloc_node->index,
                                           "iara_runtime_data__node_infos",
                                           &edge_codegen_data.alloc_node_ptr);
      if (edge_codegen_data.next_edge != nullptr)
        replace_with_element_of_global_array(edge_codegen_data.next_edge->index,
                                             "iara_runtime_data__edge_infos",
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

    // return a lambda to generate nodeinfo ptrs
    // module->dump();
    // return [&](OpBuilder builder, Location loc) {
    //   std::vector<Value> rv;
    //   for (auto [i, j] : llvm::enumerate(node_infos)) {
    //     rv.push_back(makeOffsetPointer(builder,
    //                                    loc,
    //                                    node_struct_type,
    //                                    "iara_runtime_data__node_infos",
    //                                    i));
    //   }
    //   return rv;
    // };
  }
};

CodegenStaticData::CodegenStaticData(ModuleOp module,
                                     OpBuilder module_builder,
                                     std::span<NodeCodegenData> node_pairs,
                                     std::span<EdgeCodegenData> edge_pairs) {
  pimpl = new Impl(module, module_builder, node_pairs, edge_pairs);
}

// std::function<std::vector<Value>(OpBuilder builder, Location loc)>
void CodegenStaticData::codegenStaticData() { pimpl->codegenStaticData(); }

CodegenStaticData::~CodegenStaticData() { delete pimpl; }

CodegenStaticData::CodegenStaticData(CodegenStaticData &&data) {
  pimpl = data.pimpl;
  data.pimpl = nullptr;
}

} // namespace iara::passes::virtualfifo::codegen
