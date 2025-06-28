#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/RingBuffer/Codegen/Codegen.h"
#include "Iara/Passes/RingBuffer/Codegen/GetMLIRType.h"
#include "Iara/Util/CompilerTypes.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Edge.h"
#include "IaraRuntime/ring-buffer/RingBuffer_Node.h"
#include <cassert>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Support/LLVM.h>

namespace iara::passes::ringbuffer::codegen {
using namespace iara::util::mlir;
using namespace mlir::LLVM;
using namespace iara::dialect;

// helper functions

std::string getDebugName(NodeOp node) {
  return llvm::formatv("node_{0}_{1}\0", (i64)node["id"], node.getImpl());
}

std::string getDebugName(EdgeOp edge) {
  auto prod = edge.getProducerNode();
  auto cons = edge.getConsumerNode();
  auto prod_index = util::mlir::getResultIndex(edge.getIn());
  auto cons_index = edge->getUses().begin()->getOperandNumber();
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

template <> struct GetMLIRType<RingBuffer_Node::StaticInfo> {
  static Type get(MLIRContext *context) {
    Vec<Type> node_info_struct_field_types{
        RingBuffer_Node::static_info_num_fields,
        mlir::IntegerType::get(context, 64)};
    return LLVM::LLVMStructType::getLiteral(context,
                                            node_info_struct_field_types);
  }
};

template <> struct GetMLIRType<RingBuffer_Node> {
  static Type get(MLIRContext *context) {
    auto opaque_ptr = LLVM::LLVMPointerType::get(context);
    auto span_type = LLVM::LLVMStructType::getLiteral(
        context, {opaque_ptr, IntegerType::get(context, 64)});
    return LLVM::LLVMStructType::getLiteral(
        context,
        {
            opaque_ptr,                                        // name
            getMLIRType<RingBuffer_Node::StaticInfo>(context), // info
            opaque_ptr,                                        // wrapper
            span_type,                                         // input_fifos
            span_type,                                         // output_fifos
        });
  }
};

Value asValue(OpBuilder builder,
              Location loc,
              RingBuffer_Node::StaticInfo &info) {

  using namespace iara::passes::ringbuffer::codegen;

  ArrayRef values = {
      info.id,
      info.input_bytes,
      info.num_ins,
      info.num_inouts,
      info.num_outs,
      info.working_memory_size,
      info.rank,
      info.total_iter_firings,
  };

  assert(values.size() == RingBuffer_Node::static_info_num_fields);
  Value static_info =
      CREATE(LLVM::UndefOp,
             builder,
             loc,
             getMLIRType<RingBuffer_Node::StaticInfo>(builder.getContext()));

  i64 index = 0;
  for (auto v : values) {
    static_info = CREATE(LLVM::InsertValueOp,
                         builder,
                         loc,
                         static_info,
                         createConstOp(builder, loc, v),
                         index++);
  }
  return static_info;
} // namespace iara::passes::ringbuffer::codegen

template <> struct GetMLIRType<RingBuffer_Edge::StaticInfo> {
  static Type get(MLIRContext *context) {
    Vec<Type> edge_info_struct_field_types{
        RingBuffer_Edge::static_info_num_fields,
        mlir::IntegerType::get(context, 64)};
    return LLVM::LLVMStructType::getLiteral(context,
                                            edge_info_struct_field_types);
  }
};

template <> struct GetMLIRType<RingBuffer_Edge> {
  static Type get(MLIRContext *context) {
    auto opaque_ptr = LLVM::LLVMPointerType::get(context);
    return LLVMStructType::getLiteral(
        context,
        {
            opaque_ptr,                                        // name
            getMLIRType<RingBuffer_Edge::StaticInfo>(context), // info
            getSpanType(context),                              // delay_data
            opaque_ptr,                                        // consumer
            opaque_ptr,                                        // producer
            opaque_ptr,                                        // queue
        });
  }
};

struct CodegenStaticData::Impl {

  ModuleOp module;
  std::span<RingBuffer_Node> node_infos;
  std::span<RingBuffer_Edge> edge_infos;
  std::span<NodeOp> nodes;
  std::span<EdgeOp> edges;
  std::span<LLVMFuncOp> wrappers;
  MLIRContext *ctx;
  OpBuilder module_builder;

  std::vector<Value> input_fifos_spans;
  std::vector<Value> output_fifos_spans;
  std::vector<Value> producer_node_ptrs;
  std::vector<Value> consumer_node_ptrs;

  DenseMap<RingBuffer_Edge *, EdgeOp> info_to_edge;
  DenseMap<EdgeOp, RingBuffer_Edge *> edge_to_info;

  DenseMap<RingBuffer_Node *, NodeOp> info_to_node;
  DenseMap<NodeOp, RingBuffer_Node *> node_to_info;

  Type i64type;
  Type opaque_ptr;
  // Type node_struct_type;
  // Type edge_struct_type;

  Impl(ModuleOp module,
       OpBuilder module_builder,
       std::span<RingBuffer_Node> node_infos,
       std::span<RingBuffer_Edge> edge_infos,
       std::span<NodeOp> nodes,
       std::span<EdgeOp> edges,
       std::span<LLVMFuncOp> wrappers)
      : module(module), node_infos(node_infos), edge_infos(edge_infos),
        nodes(nodes), edges(edges), wrappers(wrappers),
        ctx(module->getContext()), module_builder(module_builder) {
    i64type = module_builder.getI64Type();
    opaque_ptr = LLVMPointerType::get(ctx);

    for (auto [n, i] : llvm::zip(nodes, node_infos)) {
      info_to_node[&i] = n;
      node_to_info[n] = &i;
    }
    for (auto [e, i] : llvm::zip(edges, edge_infos)) {
      info_to_edge[&i] = e;
      edge_to_info[e] = &i;
    }
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

  Value getWrapperPtr(OpBuilder builder, Location loc, NodeOp node) {
    auto index = std::find(nodes.begin(), nodes.end(), node) - nodes.begin();
    auto wrapper = wrappers[index];
    return CREATE(AddressOfOp, builder, loc, opaque_ptr, wrapper.getSymName());
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
    Value _struct = CREATE(UndefOp, builder, loc, getSpanType(ctx));
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
                     EdgeOp edge,
                     RingBuffer_Edge &info) {
    if (info.info.delay_size == 0)
      return getEmptySpan(builder, loc);
    return makeSpan(builder,
                    loc,
                    builder.getI8Type(),
                    getEdgeDelayName(info.info.id),
                    edge_to_info[edge]->delay_data.size());
  }

  Value makeNodeInfo(OpBuilder builder,
                     RingBuffer_Node &node_info,
                     NodeOp node,
                     Location loc) {

    using namespace iara::passes::ringbuffer::codegen;

    DEF_OP(Value,
           node_info_val,
           UndefOp,
           builder,
           loc,
           getMLIRType<RingBuffer_Node>(builder.getContext()));

    auto input_fifos_placeholder = getEmptySpan(builder, loc);
    auto output_fifos_placeholder = getEmptySpan(builder, loc);

    input_fifos_spans.push_back(input_fifos_placeholder);
    output_fifos_spans.push_back(output_fifos_placeholder);

    // leaked on purpose
    auto node_name = builder.getStringAttr(getDebugName(node));

    auto name_global =
        LLVM::createGlobalString(node->getLoc(),
                                 builder,
                                 node_name,
                                 node_name,
                                 LLVM::linkage::Linkage::External,
                                 true);

    {
      Vec<Value> values = {name_global,
                           asValue(builder, loc, node_info.info),
                           getWrapperPtr(builder, loc, node),
                           input_fifos_placeholder,
                           output_fifos_placeholder};
      i64 index = 0;
      for (auto v : values) {
        node_info_val =
            CREATE(InsertValueOp, builder, loc, node_info_val, v, index++);
      }
    }
    return node_info_val;
  }

  Value makeEdgeInfo(OpBuilder builder,
                     RingBuffer_Edge &edge_info,
                     EdgeOp edge,
                     Location loc) {

    Vec<i64> static_info_data = {
        edge_info.info.id,
        edge_info.info.prod_rate,
        edge_info.info.prod_rate_aligned,
        edge_info.info.cons_rate,
        edge_info.info.cons_rate_aligned,
        edge_info.info.cons_arg_idx,
        edge_info.info.delay_size,
    };

    assert(static_info_data.size() == RingBuffer_Edge::static_info_num_fields);

    Type edge_struct_type = getMLIRType<RingBuffer_Edge>(ctx);

    Value static_info = CREATE(
        UndefOp, builder, loc, getMLIRType<RingBuffer_Edge::StaticInfo>(ctx));

    {
      i64 index = 0;
      for (auto v : static_info_data) {
        static_info = CREATE(InsertValueOp,
                             builder,
                             loc,
                             static_info,
                             createConstOp(builder, loc, v),
                             index++);
      }
    }

    Value edge_info_val = CREATE(UndefOp, builder, loc, edge_struct_type);

    auto producer = getNullPtr(builder, loc);
    auto consumer = getNullPtr(builder, loc);
    auto queue = getNullPtr(builder, loc);

    producer_node_ptrs.push_back(producer);
    consumer_node_ptrs.push_back(consumer);

    auto edge_name = builder.getStringAttr(getDebugName(edge));

    auto name_global =
        LLVM::createGlobalString(edge->getLoc(),
                                 builder,
                                 edge_name,
                                 edge_name,
                                 LLVM::linkage::Linkage::External,
                                 true);

    {
      Vec<Value> values = {name_global,
                           static_info,
                           getDelayData(builder, loc, edge, edge_info),
                           consumer,
                           producer,
                           queue};
      i64 index = 0;
      for (auto v : values) {

        edge_info_val =
            CREATE(InsertValueOp, builder, loc, edge_info_val, v, index++);
      }
    }
    // edge_info_val.getDefiningOp()->getParentOp()->dump();
    return edge_info_val;
  };

  template <size_t I> struct Index {
    const static size_t val = I;
    constexpr inline operator size_t() const { return I; }
  };

  template <class T> struct TypeWrapper {
    using type = T;
  };

  template <class I, class T> void for_each();

  template <class I = Index<0>, class T, class... Args>
  void for_each_impl(TypeWrapper<std::tuple<T, Args...>>, auto f, I i = I{}) {
    f(TypeWrapper<T>{}, i);
    if constexpr (sizeof...(Args) > 0) {
      for_each_impl(TypeWrapper<std::tuple<Args...>>{}, f, Index<I::val + 1>{});
    }
  }

  template <class... Args> void for_each(auto f) {
    for_each_impl(TypeWrapper<std::tuple<Args...>>{}, f, Index<0>{});
  };

  template <class... Args> void for_each_type(auto f) {
    size_t i = 0;
    (f(TypeWrapper<Args>{}, i++), ...);
  }

  // std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  void codegenStaticData() {
    // delays
    for (auto [i, edge, info] : llvm::enumerate(edges, edge_infos)) {
      {
        DenseArrayAttr delay_attr =
            llvm::dyn_cast_or_null<DenseArrayAttr>(edge["delay"].get());
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
                      DenseF64ArrayAttr>([&](auto type, size_t index) {
          using T = decltype(type)::type;
          auto attr = delay_attr.dyn_cast<T>();
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
                 getEdgeDelayName(info.info.id),
                 dense,
                 DataLayout::closest(edge).getTypeABIAlignment(elem_type));
        });
        assert(found);
      }
    }

    auto node_struct_type = getMLIRType<RingBuffer_Node>(ctx);
    auto edge_struct_type = getMLIRType<RingBuffer_Edge>(ctx);

    // node infos
    {
      auto [inserter, builder] =
          makeGlobalArray("iara_runtime_data__node_infos",
                          getMLIRType<RingBuffer_Node>(ctx),
                          alignof(RingBuffer_Node),
                          module.getLoc(),
                          nodes.size());

      for (auto [node, info] : llvm::zip(nodes, node_infos)) {
        auto nodeinfo = makeNodeInfo(*builder, info, node, node->getLoc());
        inserter(nodeinfo, node.getLoc());
      }
    }
    // edge infos
    {
      auto [inserter, builder] =
          makeGlobalArray("iara_runtime_data__edge_infos",
                          getMLIRType<RingBuffer_Edge>(ctx),
                          alignof(RingBuffer_Edge),
                          module.getLoc(),
                          edges.size());

      for (auto [edge, info] : llvm::zip(edges, edge_infos)) {
        inserter(makeEdgeInfo(*builder, info, edge, edge.getLoc()),
                 edge.getLoc());
      }
    }

    // node input/output fifo lists
    for (auto [i, node, node_info, old_input_span, old_output_span] :
         llvm::enumerate(
             nodes, node_infos, input_fifos_spans, output_fifos_spans)) {

      // mydump(old_span.getDefiningOp());

      for (auto edge_info : node_info.input_fifos.asSpan()) {
        assert(edge_info != nullptr);
      }

      for (auto edge_info : node_info.output_fifos.asSpan()) {
        assert(edge_info != nullptr);
      }

      auto stitch_span = [&](Span<RingBuffer_Edge *> &list,
                             const char *suffix,
                             Value old) {
        auto [inserter, builder] = makeGlobalArray(
            (std::string)llvm::formatv(
                "iara_runtime_data_node_{0}_{1}", node_info.info.id, suffix),
            opaque_ptr,
            alignof(RingBuffer_Edge),
            module.getLoc(),
            list.size());

        for (auto [i_e, edge_info] : llvm::enumerate(list.asSpan())) {
          auto edge = info_to_edge[edge_info];
          auto index = edge_info - &*edge_infos.begin();
          auto node_loc = node.getLoc();
          auto edge_loc = edge.getLoc();
          inserter(makeOffsetPointer(*builder,
                                     node_loc,
                                     edge_struct_type,
                                     "iara_runtime_data__edge_infos",
                                     index),
                   edge_loc);
        }

        auto node_struct_builder = OpBuilder(old.getDefiningOp());
        auto loc = node.getLoc();
        Value new_span = makeSpan(
            node_struct_builder,
            loc,
            opaque_ptr,
            (std::string)llvm::formatv(
                "iara_runtime_data_node_{0}_{1}", node_info.info.id, suffix),
            list.asSpan().size());
        old.replaceAllUsesWith(new_span);
        old.getDefiningOp()->erase();
        assert(module.verify().succeeded());
      };

      stitch_span(node_info.input_fifos, "input_fifos", old_input_span);
      stitch_span(node_info.output_fifos, "output_fifos", old_output_span);
    }

    // edge consumers, producers and next pointers
    for (auto [i, edge, info, old_producer, old_consumer] : llvm::enumerate(
             edges, edge_infos, producer_node_ptrs, consumer_node_ptrs)) {

      auto replace = [&](RingBuffer_Node *node, Value old) {
        auto index = node - &*node_infos.begin();
        auto builder = OpBuilder(old.getDefiningOp());
        auto loc = old.getLoc();
        auto _new = makeOffsetPointer(builder,
                                      loc,
                                      node_struct_type,
                                      "iara_runtime_data__node_infos",
                                      index);
        old.replaceAllUsesWith(_new);
        old.getDefiningOp()->erase();
      };

      replace(info.producer, old_producer);
      replace(info.consumer, old_consumer);
    }

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
                                       nodes.size());
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
                                       edges.size());
                     });
  }
};

CodegenStaticData::CodegenStaticData(ModuleOp module,
                                     OpBuilder module_builder,
                                     std::span<RingBuffer_Node> node_infos,
                                     std::span<RingBuffer_Edge> edge_infos,
                                     std::span<NodeOp> nodes,
                                     std::span<EdgeOp> edges,
                                     std::span<LLVMFuncOp> wrappers) {
  pimpl = new Impl(
      module, module_builder, node_infos, edge_infos, nodes, edges, wrappers);
}

// std::function<std::vector<Value>(OpBuilder builder, Location loc)>
void CodegenStaticData::codegenStaticData() { pimpl->codegenStaticData(); }

CodegenStaticData::~CodegenStaticData() { delete pimpl; }

CodegenStaticData::CodegenStaticData(CodegenStaticData &&data) {
  pimpl = data.pimpl;
  data.pimpl = nullptr;
}

} // namespace iara::passes::ringbuffer::codegen
