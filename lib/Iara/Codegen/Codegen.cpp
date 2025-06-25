#include "Iara/Codegen/Codegen.h"
#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Span.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include <cassert>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Support/LLVM.h>

namespace iara::codegen {
using namespace iara::util::mlir;
using namespace mlir::LLVM;
using namespace iara::dialect;

// helper functions

std::string getDebugName(NodeOp node, i64 id) {
  if (node.isAlloc()) {
    auto alloc_edge = cast<EdgeOp>(*node->getResult(0).getUsers().begin());
    auto first_node = alloc_edge.getConsumerNode();
    auto index = alloc_edge->getUses().begin()->getOperandNumber();
    return llvm::formatv(
        "allocNode_{0}[{1}]", getDebugName(first_node, id), index);
  }
  if (node.isDealloc()) {
    auto dealloc_edge = cast<EdgeOp>(node->getOperand(0).getDefiningOp());
    auto last_node = dealloc_edge.getProducerNode();
    auto index = util::mlir::getResultIndex(dealloc_edge.getIn());
    return llvm::formatv(
        "deallocNode_{0}[{1}]", getDebugName(last_node, id), index);
  }
  return llvm::formatv("node_{0}_{1}", id, node.getImpl());
}

std::string getDebugName(EdgeOp edge, i64 id) {
  auto prod = edge.getProducerNode();
  auto cons = edge.getConsumerNode();
  auto prod_index = util::mlir::getResultIndex(edge.getIn());
  auto cons_index = edge->getUses().begin()->getOperandNumber();
  if (prod.isAlloc()) {
    return llvm::formatv("allocEdge_{0}[{1}]", cons.getImpl(), cons_index);
  }
  if (cons.isDealloc()) {
    return llvm::formatv("deallocNode_{0}[{1}]", prod.getImpl(), prod_index);
  }
  return llvm::formatv("edge_{0}[{1}]->{2}[{3}]",
                       prod.getImpl(),
                       prod_index,
                       cons.getImpl(),
                       cons_index);
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

template <> struct GetMLIRType<SDF_OoO_Node::StaticInfo> {
  static Type get(MLIRContext *context) {
    Vec<Type> node_info_struct_field_types{SDF_OoO_Node::static_info_num_fields,
                                           mlir::IntegerType::get(context, 64)};
    return LLVM::LLVMStructType::getLiteral(context,
                                            node_info_struct_field_types);
  }
};

template <> struct GetMLIRType<SDF_OoO_Node> {
  static Type get(MLIRContext *context) {
    auto opaque_ptr = LLVM::LLVMPointerType::get(context);
    auto span_type = LLVM::LLVMStructType::getLiteral(
        context, {opaque_ptr, IntegerType::get(context, 64)});
    return LLVM::LLVMStructType::getLiteral(
        context,
        {
            opaque_ptr,                                     // name
            getMLIRType<SDF_OoO_Node::StaticInfo>(context), // info
            opaque_ptr,                                     // wrapper
            span_type,                                      // input_fifos
            span_type,                                      // output_fifos
            opaque_ptr,                                     // semaphore
        });
  }
};

Value asValue(OpBuilder builder, Location loc, SDF_OoO_Node::StaticInfo &info) {

  using namespace iara::codegen;

  ArrayRef values = {info.id,
                     info.input_bytes,
                     info.num_inputs,
                     info.rank,
                     info.total_iter_firings,
                     info.needs_priming};

  assert(values.size() == SDF_OoO_Node::static_info_num_fields);
  Value static_info =
      CREATE(LLVM::UndefOp,
             builder,
             loc,
             iara::codegen::getMLIRType<SDF_OoO_Node::StaticInfo>(
                 builder.getContext()));

  i64 index = 0;
  for (auto v : values) {
    static_info = CREATE(LLVM::InsertValueOp,
                         builder,
                         loc,
                         static_info,
                         ::iara::codegen::createConstOp(builder, loc, v),
                         index++);
  }
  return static_info;
}

template <> struct GetMLIRType<SDF_OoO_FIFO::StaticInfo> {
  static Type get(MLIRContext *context) {
    Vec<Type> edge_info_struct_field_types{SDF_OoO_FIFO::static_info_num_fields,
                                           mlir::IntegerType::get(context, 64)};
    return LLVM::LLVMStructType::getLiteral(context,
                                            edge_info_struct_field_types);
  }
};

template <> struct GetMLIRType<SDF_OoO_FIFO> {
  static Type get(MLIRContext *context) {
    auto opaque_ptr = LLVM::LLVMPointerType::get(context);
    return LLVMStructType::getLiteral(
        context,
        {
            opaque_ptr,                                     // name
            getMLIRType<SDF_OoO_FIFO::StaticInfo>(context), // info
            getSpanType(context),                           // delay_data
            opaque_ptr,                                     // consumer
            opaque_ptr,                                     // producer
            opaque_ptr,                                     // alloc_node
            opaque_ptr                                      // next_in_chain
        });
  }
};

struct CodegenStaticData::Impl {

  ModuleOp module;
  std::span<SDF_OoO_Node> node_infos;
  std::span<SDF_OoO_FIFO> edge_infos;
  std::span<NodeOp> nodes;
  std::span<EdgeOp> edges;
  std::span<LLVMFuncOp> wrappers;
  MLIRContext *ctx;
  OpBuilder module_builder;

  std::vector<Value> input_fifos_spans;
  std::vector<Value> output_fifos_spans;
  std::vector<Value> producer_node_ptrs;
  std::vector<Value> consumer_node_ptrs;
  std::vector<Value> alloc_node_ptrs;
  std::vector<Value> next_edge_ptrs;

  DenseMap<SDF_OoO_FIFO *, EdgeOp> info_to_edge;
  DenseMap<EdgeOp, SDF_OoO_FIFO *> edge_to_info;

  DenseMap<SDF_OoO_Node *, NodeOp> info_to_node;
  DenseMap<NodeOp, SDF_OoO_Node *> node_to_info;

  Type i64type;
  Type opaque_ptr;
  // Type node_struct_type;
  // Type edge_struct_type;

  Impl(ModuleOp module,
       OpBuilder module_builder,
       std::span<SDF_OoO_Node> node_infos,
       std::span<SDF_OoO_FIFO> edge_infos,
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

  Value buildWrapper(OpBuilder builder, Location loc, NodeOp node) {
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
                     SDF_OoO_FIFO &info) {
    if (info.info.delay_size == 0)
      return getEmptySpan(builder, loc);
    return makeSpan(builder,
                    loc,
                    builder.getI8Type(),
                    getEdgeDelayName(info.info.id),
                    edge_to_info[edge]->delay_data.size());
  }

  Value makeNodeInfo(OpBuilder builder,
                     SDF_OoO_Node &node_info,
                     NodeOp node,
                     Location loc) {

    using namespace iara::codegen;

    DEF_OP(Value,
           node_info_val,
           UndefOp,
           builder,
           loc,
           getMLIRType<SDF_OoO_Node>(builder.getContext()));

    auto input_fifos_placeholder = getEmptySpan(builder, loc);
    auto output_fifos_placeholder = getEmptySpan(builder, loc);

    input_fifos_spans.push_back(input_fifos_placeholder);
    output_fifos_spans.push_back(output_fifos_placeholder);

    // leaked on purpose
    auto node_name =
        builder.getStringAttr(getDebugName(node, node_info.info.id));

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
                           buildWrapper(builder, loc, node),
                           input_fifos_placeholder,
                           output_fifos_placeholder,
                           getNullPtr(builder, loc)};
      i64 index = 0;
      for (auto v : values) {
        node_info_val =
            CREATE(InsertValueOp, builder, loc, node_info_val, v, index++);
      }
    }
    return node_info_val;
  }

  Value makeEdgeInfo(OpBuilder builder,
                     SDF_OoO_FIFO &edge_info,
                     EdgeOp edge,
                     Location loc) {

    Vec<i64> static_info_data = {
        edge_info.info.id,
        edge_info.info.local_index,
        edge_info.info.prod_rate,
        edge_info.info.cons_rate,
        edge_info.info.cons_arg_idx,
        edge_info.info.delay_offset,
        edge_info.info.delay_size,
        edge_info.info.block_size_with_delays,
        edge_info.info.block_size_no_delays,
        edge_info.info.prod_alpha,
        edge_info.info.prod_beta,
        edge_info.info.cons_alpha,
        edge_info.info.cons_beta,
    };

    assert(static_info_data.size() == SDF_OoO_FIFO::static_info_num_fields);

    Type edge_struct_type = getMLIRType<SDF_OoO_FIFO>(ctx);

    Value static_info = CREATE(
        UndefOp, builder, loc, getMLIRType<SDF_OoO_FIFO::StaticInfo>(ctx));

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
    auto alloc_node = getNullPtr(builder, loc);
    auto next_in_chain = getNullPtr(builder, loc);

    producer_node_ptrs.push_back(producer);
    consumer_node_ptrs.push_back(consumer);
    alloc_node_ptrs.push_back(alloc_node);
    next_edge_ptrs.push_back(next_in_chain);

    auto edge_name =
        builder.getStringAttr(getDebugName(edge, edge_info.info.id));

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
                           alloc_node,
                           next_in_chain};
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

    auto node_struct_type = getMLIRType<SDF_OoO_Node>(ctx);
    auto edge_struct_type = getMLIRType<SDF_OoO_FIFO>(ctx);

    // node infos
    {
      auto [inserter, builder] =
          makeGlobalArray("iara_runtime_data__node_infos",
                          getMLIRType<SDF_OoO_Node>(ctx),
                          alignof(SDF_OoO_Node),
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
                          getMLIRType<SDF_OoO_FIFO>(ctx),
                          alignof(SDF_OoO_FIFO),
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

      auto stitch_span = [&](Span<SDF_OoO_FIFO *> &list,
                             const char *suffix,
                             Value old) {
        auto [inserter, builder] = makeGlobalArray(
            (std::string)llvm::formatv(
                "iara_runtime_data_node_{0}_{1}", node_info.info.id, suffix),
            opaque_ptr,
            alignof(SDF_OoO_FIFO),
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
    for (auto [i,
               edge,
               info,
               old_producer,
               old_consumer,
               old_alloc_node,
               old_next] : llvm::enumerate(edges,
                                           edge_infos,
                                           producer_node_ptrs,
                                           consumer_node_ptrs,
                                           alloc_node_ptrs,
                                           next_edge_ptrs)) {

      auto replace = [&](SDF_OoO_Node *node, Value old) {
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
      replace(info.alloc_node, old_alloc_node);

      {
        auto builder = OpBuilder(old_next.getDefiningOp());
        auto loc = old_next.getLoc();
        auto next_edge = followInoutChainForwards(edge);
        if (next_edge) {
          auto next_info = edge_to_info[next_edge];
          auto next_index = next_info - &*edge_infos.begin();
          auto new_next = makeOffsetPointer(builder,
                                            loc,
                                            edge_struct_type,
                                            "iara_runtime_data__edge_infos",
                                            next_index);
          old_next.replaceAllUsesWith(new_next);
          old_next.getDefiningOp()->erase();
        }
      }
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
                                       nodes.size());
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
                                     std::span<SDF_OoO_Node> node_infos,
                                     std::span<SDF_OoO_FIFO> edge_infos,
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

} // namespace iara::codegen
