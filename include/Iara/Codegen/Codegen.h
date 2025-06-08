#ifndef IARA_UTIL_CODEGEN_H
#define IARA_UTIL_CODEGEN_H

#include "Iara/Codegen/GetMLIRType.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Util/Mlir.h"
#include "Iara/Util/OpCreateHelper.h"
#include "Iara/Util/Types.h"
#include "IaraRuntime/SDF_OoO_FIFO.h"
#include "IaraRuntime/SDF_OoO_Node.h"
#include "mlir/IR/Builders.h"
#include <bits/types/locale_t.h>
#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/FormatVariadic.h>
#include <memory>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Dominance.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/TypeUtilities.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Value.h>
#include <mlir/Interfaces/DataLayoutInterfaces.h>
#include <type_traits>

namespace iara::codegen {
using namespace util::mlir;
using namespace LLVM;
using namespace iara::dialect;

struct CodegenStaticData {

  ModuleOp module;
  OpBuilder module_builder;
  std::span<SDF_OoO_Node> node_infos;
  std::span<SDF_OoO_FIFO> edge_infos;
  std::span<NodeOp> nodes;
  std::span<EdgeOp> edges;
  std::span<LLVMFuncOp> wrappers;

  MLIRContext *ctx;

  std::vector<Value> output_fifos_spans;
  std::vector<Value> producer_node_ptrs;
  std::vector<Value> consumer_node_ptrs;

  DenseMap<SDF_OoO_FIFO *, EdgeOp> info_to_edge;
  DenseMap<EdgeOp, SDF_OoO_FIFO *> edge_to_info;

  DenseMap<SDF_OoO_Node *, NodeOp> info_to_node;
  DenseMap<NodeOp, SDF_OoO_Node *> node_to_info;

  Type i64type;
  Type opaque_ptr;
  Type span_type;
  Type node_struct_type;
  Type edge_struct_type;
  Type node_static_info_struct_type;
  Type edge_static_info_struct_type;

  CodegenStaticData(ModuleOp module, OpBuilder module_builder,
                    std::span<SDF_OoO_Node> node_infos,
                    std::span<SDF_OoO_FIFO> edge_infos, std::span<NodeOp> nodes,
                    std::span<EdgeOp> edges, std::span<LLVMFuncOp> wrappers)
      : module(module), module_builder(module_builder), node_infos(node_infos),
        edge_infos(edge_infos), nodes(nodes), edges(edges), wrappers(wrappers),
        ctx(module->getContext()) {
    i64type = module_builder.getI64Type();
    opaque_ptr = LLVMPointerType::get(ctx);
    span_type = LLVMStructType::getLiteral(ctx, {opaque_ptr, i64type});

    for (auto [n, i] : llvm::zip(nodes, node_infos)) {
      info_to_node[&i] = n;
      node_to_info[n] = &i;
    }
    for (auto [e, i] : llvm::zip(edges, edge_infos)) {
      info_to_edge[&i] = e;
      edge_to_info[e] = &i;
    }
  }

  GlobalOp
  makeGlobalStruct(OpBuilder builder, Location loc, StringRef name, Type type,
                   bool is_constant,
                   std::function<Value(OpBuilder, Location)> inserter) {
    auto rv =
        CREATE(GlobalOp, builder, loc, type, is_constant, Linkage::External,
               name, {}, DataLayout::closest(module).getTypeABIAlignment(type));
    rv.getInitializer().emplaceBlock();
    auto init_builder = OpBuilder::atBlockBegin(rv.getInitializerBlock());
    auto val = inserter(init_builder, loc);
    CREATE(ReturnOp, init_builder, loc, val);
    return rv;
  }

  std::pair<std::function<void(Value, Location)>, std::shared_ptr<OpBuilder>>
  makeGlobalArray(StringRef name, Type elem_type, i64 align, Location loc,
                  i64 size) {
    OpBuilder builder = OpBuilder::atBlockBegin(module.getBody());
    auto array_type = LLVMArrayType::get(elem_type, size);
    auto global_op = CREATE(GlobalOp, builder, loc, array_type, false,
                            Linkage::External, name, {}, align);
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
      auto insert_op = CREATE(InsertValueOp, *values_builder, loc, wip_value,
                              new_value, index++);
      Value new_struct = insert_op.getResult();
      wip_value.replaceAllUsesExcept(new_struct, insert_op);
      wip_value = new_struct;
      assert(global_op.verify().succeeded());
    };

    return {lambda, values_builder};
  }

  Value asValue(OpBuilder builder, Location loc, auto val) {
    return CREATE(ConstantOp, builder, loc, getMLIRType<decltype(val)>(ctx),
                  val);
  }

  Value getWrapper(OpBuilder builder, Location loc, NodeOp node) {
    auto index = std::find(nodes.begin(), nodes.end(), node) - nodes.begin();
    auto wrapper = wrappers[index];
    return CREATE(AddressOfOp, builder, loc, opaque_ptr, wrapper.getSymName());
  }

  Value getNullPtr(OpBuilder builder, Location loc) {
    return CREATE(ZeroOp, builder, loc, opaque_ptr);
  }

  Value getEmptySpan(OpBuilder builder, Location loc) {
    Value _struct = CREATE(UndefOp, builder, loc, span_type);
    _struct = CREATE(InsertValueOp, builder, loc, _struct,
                     getNullPtr(builder, loc), ArrayRef<i64>{0});
    _struct = CREATE(InsertValueOp, builder, loc, _struct,
                     asValue(builder, loc, i64(0)), ArrayRef<i64>{1});
    return _struct;
  }

  Value makeOffsetPointer(OpBuilder builder, Location loc, Type elem_type,
                          StringRef name, i64 index) {
    Value first = CREATE(AddressOfOp, builder, loc, opaque_ptr, name);
    llvm::errs() << getTypeSize(elem_type, DataLayout::closest(module));
    Value gep =
        CREATE(GEPOp, builder, loc, opaque_ptr, elem_type, first, {index});
    return gep;
  }

  Value makeSpan(OpBuilder builder, Location loc, Type elem_type,
                 StringRef name, i64 size) {
    Value _struct = CREATE(UndefOp, builder, loc, span_type);
    auto ptr = makeOffsetPointer(builder, loc, elem_type, name, 0);
    _struct =
        CREATE(InsertValueOp, builder, loc, _struct, ptr, ArrayRef<i64>{0});
    _struct = CREATE(InsertValueOp, builder, loc, _struct,
                     asValue(builder, loc, size), ArrayRef<i64>{1});
    return _struct;
  }

  Type getEdgeElementType(EdgeOp edge) {
    return getElementTypeOrSelf(edge.getIn().getType());
  }

  std::string getEdgeDelayName(i64 index) {
    return (std::string)llvm::formatv("iara_runtime_data_delay_{0}", index);
  }

  Value getDelayData(OpBuilder builder, Location loc, EdgeOp edge,
                     SDF_OoO_FIFO &info) {
    auto index = std::find(edges.begin(), edges.end(), edge) - edges.begin();

    if (info.info.delay_size == 0)
      return getEmptySpan(builder, loc);
    return makeSpan(builder, loc, builder.getI8Type(), getEdgeDelayName(index),
                    edge_to_info[edge]->delay_data.size());
  }

  Value makeNodeInfo(OpBuilder builder, SDF_OoO_Node &node_info, NodeOp node,
                     Location loc) {

    if (!node_static_info_struct_type)
      node_static_info_struct_type = LLVMStructType::getLiteral(
          ctx, {i64type, i64type, i64type, i64type, i64type});

    if (!node_struct_type)
      node_struct_type =
          LLVMStructType::getLiteral(ctx, {node_static_info_struct_type,
                                           opaque_ptr, span_type, opaque_ptr});

    Value static_info =
        CREATE(UndefOp, builder, loc, node_static_info_struct_type);

    {
      Vec<i64> values = {node_info.info.id, node_info.info.input_bytes,
                         node_info.info.num_inputs, node_info.info.rank,
                         node_info.info.total_firings};

      i64 index = 0;
      for (auto v : values) {
        static_info = CREATE(InsertValueOp, builder, loc, static_info,
                             asValue(builder, loc, v), index++);
      }
    }

    Value node_info_val = CREATE(UndefOp, builder, loc, node_struct_type);

    auto output_fifos_placeholder = getEmptySpan(builder, loc);

    output_fifos_spans.push_back(output_fifos_placeholder);

    {
      Vec<Value> values = {static_info, getWrapper(builder, loc, node),
                           output_fifos_placeholder, getNullPtr(builder, loc)};
      i64 index = 0;
      for (auto v : values) {
        node_info_val =
            CREATE(InsertValueOp, builder, loc, node_info_val, v, index++);
      }
    }
    return node_info_val;
  };

  Value makeEdgeInfo(OpBuilder builder, SDF_OoO_FIFO &edge_info, EdgeOp edge,
                     Location loc) {

    Vec<i64> static_info_data = {
        edge_info.info.id,
        edge_info.info.local_index,
        edge_info.info.prod_rate,
        edge_info.info.cons_rate,
        edge_info.info.cons_arg_idx,
        edge_info.info.delay_offset,
        edge_info.info.delay_size,
        edge_info.info.first_chunk_size,
        edge_info.info.next_chunk_sizes,
        edge_info.info.prod_alpha,
        edge_info.info.prod_beta,
        edge_info.info.cons_alpha,
        edge_info.info.cons_beta,
    };

    Vec<Type> static_info_types;

    for (auto _ : static_info_data) {
      static_info_types.push_back(i64type);
    }

    if (!edge_static_info_struct_type)

      edge_static_info_struct_type =
          LLVMStructType::getLiteral(ctx, static_info_types);

    if (!edge_struct_type)
      edge_struct_type = LLVMStructType::getLiteral(
          ctx, {
                   edge_static_info_struct_type, // info
                   span_type,                    // delay_data
                   opaque_ptr,                   // consumer
                   opaque_ptr                    // producer
               });

    Value static_info =
        CREATE(UndefOp, builder, loc, edge_static_info_struct_type);

    {
      i64 index = 0;
      for (auto v : static_info_data) {
        static_info = CREATE(InsertValueOp, builder, loc, static_info,
                             asValue(builder, loc, v), index++);
      }
    }

    Value edge_info_val = CREATE(UndefOp, builder, loc, edge_struct_type);

    auto producer = getNullPtr(builder, loc);
    auto consumer = getNullPtr(builder, loc);

    producer_node_ptrs.push_back(producer);
    consumer_node_ptrs.push_back(consumer);

    {
      Vec<Value> values = {static_info,
                           getDelayData(builder, loc, edge, edge_info),
                           consumer, producer};
      i64 index = 0;
      for (auto v : values) {

        edge_info_val =
            CREATE(InsertValueOp, builder, loc, edge_info_val, v, index++);
      }
    }
    edge_info_val.getDefiningOp()->getParentOp()->dump();
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

  std::function<std::vector<Value>(OpBuilder builder, Location loc)>
  codegenStaticData() {
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

        for_each_type<DenseI8ArrayAttr, DenseI16ArrayAttr, DenseI32ArrayAttr,
                      DenseI64ArrayAttr, DenseF32ArrayAttr, DenseF64ArrayAttr>(
            [&](auto type, size_t index) {
              using T = decltype(type)::type;
              if (auto attr = delay_attr.dyn_cast<T>()) {
                assert(found == false);
                found = true;
                auto span = attr.asArrayRef();
                for (auto i : span) {
                  CREATE(
                      GlobalOp, module_builder, edge.getLoc(),
                      LLVMArrayType::get(attr.getElementType(), span.size()),
                      false, Linkage::External, getEdgeDelayName(i), attr,
                      DataLayout::closest(edge).getTypeABIAlignment(elem_type));
                }
              }
            });
        assert(found);
      }
    }

    // node infos
    {
      auto [inserter, builder] = makeGlobalArray(
          "iara_runtime_data__node_infos", getMLIRType<SDF_OoO_Node>(ctx),
          alignof(SDF_OoO_Node), module.getLoc(), nodes.size());

      for (auto [node, info] : llvm::zip(nodes, node_infos)) {
        inserter(makeNodeInfo(*builder, info, node, node->getLoc()),
                 node.getLoc());
      }
    }
    // edge infos
    {
      auto [inserter, builder] = makeGlobalArray(
          "iara_runtime_data__edge_infos", getMLIRType<SDF_OoO_FIFO>(ctx),
          alignof(SDF_OoO_FIFO), module.getLoc(), edges.size());

      for (auto [edge, info] : llvm::zip(edges, edge_infos)) {
        inserter(makeEdgeInfo(*builder, info, edge, edge.getLoc()),
                 edge.getLoc());
      }
    }

    // node output fifo lists
    for (auto [i, node, info, old_span] :
         llvm::enumerate(nodes, node_infos, output_fifos_spans)) {

      auto [inserter, builder] =
          makeGlobalArray((std::string)llvm::formatv(
                              "iara_runtime_data_node_{0}_output_fifos", i),
                          opaque_ptr, alignof(SDF_OoO_FIFO), module.getLoc(),
                          info.output_fifos.size());

      for (auto [i_e, edge_info] :
           llvm::enumerate(info.output_fifos.asSpan())) {
        auto edge = info_to_edge[edge_info];
        auto index = edge_info - &*edge_infos.begin();
        inserter(makeOffsetPointer(*builder, node.getLoc(), edge_struct_type,
                                   "iara_runtime_data__edge_infos", index),
                 edge.getLoc());
      }

      auto node_struct_builder = OpBuilder(old_span.getDefiningOp());
      auto loc = node.getLoc();
      Value new_span =
          makeSpan(node_struct_builder, loc, opaque_ptr,
                   (std::string)llvm::formatv(
                       "iara_runtime_data_node_{0}_output_fifos", i),
                   info.output_fifos.asSpan().size());
      old_span.replaceAllUsesWith(new_span);
      old_span.getDefiningOp()->erase();
      assert(module.verify().succeeded());
    }

    // edge consumers and producers
    for (auto [i, edge, info, old_producer, old_consumer] : llvm::enumerate(
             edges, edge_infos, producer_node_ptrs, consumer_node_ptrs)) {

      auto prod_index = info.producer - &*node_infos.begin();
      auto cons_index = info.consumer - &*node_infos.begin();

      auto builder = OpBuilder(old_producer.getDefiningOp());
      auto loc = old_producer.getLoc();
      auto new_producer =
          makeOffsetPointer(builder, loc, node_struct_type,
                            "iara_runtime_data__node_infos", prod_index);
      auto new_consumer =
          makeOffsetPointer(builder, loc, node_struct_type,
                            "iara_runtime_data__node_infos", cons_index);
      old_producer.replaceAllUsesWith(new_producer);
      old_consumer.replaceAllUsesWith(new_consumer);
      old_producer.getDefiningOp()->erase();
      old_consumer.getDefiningOp()->erase();
    }

    // make global spans for debugging

    makeGlobalStruct(
        module_builder, module.getLoc(), "iara_runtime_nodes", span_type, true,
        [&](OpBuilder builder, Location loc) -> Value {
          return makeSpan(builder, loc, node_struct_type,
                          "iara_runtime_data__node_infos", nodes.size());
        });

    makeGlobalStruct(
        module_builder, module.getLoc(), "iara_runtime_edges", span_type, true,
        [&](OpBuilder builder, Location loc) -> Value {
          return makeSpan(builder, loc, edge_struct_type,
                          "iara_runtime_data__edge_infos", nodes.size());
        });

    // return a lambda to generate nodeinfo ptrs
    module->dump();
    return [&](OpBuilder builder, Location loc) {
      std::vector<Value> rv;
      for (auto [i, j] : llvm::enumerate(node_infos)) {
        rv.push_back(makeOffsetPointer(builder, loc, node_struct_type,
                                       "iara_runtime_data__node_infos", i));
      }
      return rv;
    };
  }
};

} // namespace iara::codegen
#endif
