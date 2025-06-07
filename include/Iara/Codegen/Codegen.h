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
#include <llvm/Support/FormatVariadic.h>
#include <mlir/Dialect/LLVMIR/LLVMAttrs.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/LLVMIR/LLVMTypes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
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
  std::span<SDF_OoO_Node> node_infos;
  std::span<SDF_OoO_FIFO> edge_infos;
  std::span<NodeOp> nodes;
  std::span<EdgeOp> edges;
  std::span<LLVMFuncOp> wrappers;
  MLIRContext *ctx;
  OpBuilder module_builder;

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

  CodegenStaticData(ModuleOp module, std::span<SDF_OoO_Node> node_infos,
                    std::span<SDF_OoO_FIFO> edge_infos,
                    std::vector<NodeOp> nodes, std::vector<EdgeOp> edges,
                    std::vector<LLVMFuncOp> wrappers)
      : module(module), node_infos(node_infos), edge_infos(edge_infos),
        nodes(nodes), edges(edges), wrappers(wrappers),
        ctx(module->getContext()),
        module_builder(OpBuilder::atBlockBegin(module.getBody())) {
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

  std::pair<std::function<void(Value, Location)>, OpBuilder>
  makeCompoundGlobal(StringRef name, Type type, i64 align, Location loc) {

    OpBuilder builder = OpBuilder::atBlockBegin(module.getBody());
    auto global_op = CREATE(LLVM::GlobalOp, builder, loc, type, false,
                            LLVM::Linkage::External, name, {}, align);
    global_op.getInitializer().emplaceBlock();
    auto global_init_builder =
        OpBuilder::atBlockBegin(global_op.getInitializerBlock());

    Value value = CREATE(LLVM::UndefOp, global_init_builder, loc, type);

    auto return_op = CREATE(LLVM::ReturnOp, global_init_builder, loc, value);

    i64 index = 0;

    return {[&](Value new_value, Location loc) -> Value {
              value.dropAllUses();
              Value new_struct =
                  CREATE(InsertValueOp, builder, loc, value, new_value, index);
              index++;
              return_op->insertOperands(0, new_struct);
            },
            OpBuilder(return_op)};
  }

  Value asValue(OpBuilder builder, Location loc, auto val) {
    return CREATE(ConstantOp, builder, loc, getMLIRType<decltype(val)>(ctx),
                  val);
  }

  Value getWrapper(OpBuilder builder, Location loc, NodeOp node) {
    auto index = std::find(nodes.begin(), nodes.end(), node) - nodes.begin();
    auto wrapper = wrappers[index];
    return CREATE(AddressOfOp, builder, loc, wrapper);
  }

  Value getNullPtr(OpBuilder builder, Location loc) {
    return CREATE(ZeroOp, builder, loc, opaque_ptr);
  }

  Value getEmptyPtrSpan(OpBuilder builder, Location loc) {
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
    Value gep = CREATE(GEPOp, builder, loc, opaque_ptr,
                       LLVMPointerType::get(elem_type), first, {index});
    return gep;
  }

  Value makeSpan(OpBuilder builder, Location loc, Type elem_type,
                 StringRef name, i64 size) {
    Value _struct = CREATE(UndefOp, builder, loc, span_type);
    auto ptr = makeOffsetPointer(builder, loc, elem_type, name, 0);
    _struct =
        CREATE(InsertValueOp, builder, loc, _struct, ptr, ArrayRef<i64>{0});
    _struct = CREATE(InsertValueOp, builder, loc, _struct,
                     asValue(builder, loc, size), ArrayRef<i64>{0});
    return _struct;
  }

  Type getEdgeElementType(EdgeOp edge) {
    return getElementTypeOrSelf(edge.getIn().getType());
  }

  std::string getEdgeDelayName(i64 index) {
    return (std::string)llvm::formatv("iara_runtime_data_delay_{0}", index);
  }

  Value getDelayData(OpBuilder builder, Location loc, EdgeOp edge) {
    auto index = std::find(edges.begin(), edges.end(), edge) - edges.begin();

    return makeSpan(builder, loc, getEdgeElementType(edge),
                    getEdgeDelayName(index),
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

    auto output_fifos_placeholder = getEmptyPtrSpan(builder, loc);

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
        edge_info.info.local_index,      edge_info.info.prod_rate,
        edge_info.info.cons_rate,        edge_info.info.cons_arg_idx,
        edge_info.info.delay_offset,     edge_info.info.delay_size,
        edge_info.info.first_chunk_size, edge_info.info.next_chunk_sizes,
        edge_info.info.prod_alpha,       edge_info.info.prod_beta,
        edge_info.info.cons_alpha,       edge_info.info.cons_beta,
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
          ctx, {edge_static_info_struct_type, opaque_ptr, span_type, opaque_ptr,
                opaque_ptr});

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
      Vec<Value> values = {static_info, getDelayData(builder, loc, edge),
                           consumer, producer};
      i64 index = 0;
      for (auto v : values) {
        edge_info_val =
            CREATE(InsertValueOp, builder, loc, edge_info_val, v, index++);
      }
    }
    return edge_info_val;
  };

  std::function<std::vector<Value>(OpBuilder builder, Location loc)> codegen() {
    {
      for (auto [i, edge, info] : llvm::enumerate(edges, edge_infos)) {
        {
          auto elem_type = getElementTypeOrSelf(edge.getIn().getType());

          auto [inserter, builder] = makeCompoundGlobal(
              getEdgeDelayName(i),
              LLVMArrayType::get(elem_type, info.delay_data.size()),
              DataLayout::closest(edge).getTypeABIAlignment(elem_type),
              edge.getLoc());

          for (auto [node, info] : llvm::zip(nodes, node_infos)) {
            inserter(makeNodeInfo(builder, info, node, node->getLoc()),
                     node.getLoc());
          }
        }
      }
    }

    {
      auto [inserter, builder] = makeCompoundGlobal(
          "iara_runtime_data__node_infos",
          LLVMArrayType::get(getMLIRType<SDF_OoO_Node>(ctx), nodes.size()),
          alignof(SDF_OoO_Node), module.getLoc());

      for (auto [node, info] : llvm::zip(nodes, node_infos)) {
        inserter(makeNodeInfo(builder, info, node, node->getLoc()),
                 node.getLoc());
      }
    }
    {
      auto [inserter, builder] = makeCompoundGlobal(
          "iara_runtime_data__edge_infos",
          LLVMArrayType::get(getMLIRType<SDF_OoO_FIFO>(ctx), edges.size()),
          alignof(SDF_OoO_FIFO), module.getLoc());

      for (auto [edge, info] : llvm::zip(edges, edge_infos)) {
        inserter(makeEdgeInfo(builder, info, edge, edge.getLoc()),
                 edge.getLoc());
      }
    }

    for (auto [i, node, info, old_span] :
         llvm::enumerate(nodes, node_infos, output_fifos_spans)) {
      {
        auto [inserter, builder] = makeCompoundGlobal(
            (std::string)llvm::formatv(
                "iara_runtime_data_node_{0}_output_fifos"),
            LLVMArrayType::get(opaque_ptr, info.output_fifos.extents),
            alignof(SDF_OoO_FIFO), module.getLoc());

        for (auto [i, info] : llvm::enumerate(info.output_fifos.asSpan())) {
          auto edge = info_to_edge[info];
          auto index = info - &*edge_infos.begin();
          inserter(makeOffsetPointer(builder, node.getLoc(), edge_struct_type,
                                     "iara_runtime_data__edge_infos", index),
                   edge.getLoc());
        }
      }
      auto builder = OpBuilder(old_span.getDefiningOp());
      auto loc = node.getLoc();
      Value new_span = makeSpan(builder, loc, opaque_ptr,
                                "iara_runtime_data_node_{0}_output_fifos",
                                info.output_fifos.asSpan().size());
      old_span.replaceAllUsesWith(new_span);
      old_span.getDefiningOp()->erase();
    }

    for (auto [i, edge, info, old_producer, old_consumer] : llvm::enumerate(
             edges, edge_infos, producer_node_ptrs, consumer_node_ptrs)) {

      auto prod_index = info.producer - &*node_infos.begin();
      auto cons_index = info.consumer - &*node_infos.begin();

      auto builder = OpBuilder(old_producer.getDefiningOp());
      auto loc = old_producer.getLoc();
      auto new_producer =
          makeOffsetPointer(builder, loc, opaque_ptr,
                            "iara_runtime_data__node_infos", prod_index);
      auto new_consumer =
          makeOffsetPointer(builder, loc, opaque_ptr,
                            "iara_runtime_data__node_infos", cons_index);
      old_producer.replaceAllUsesWith(new_producer);
      old_consumer.replaceAllUsesWith(new_consumer);
      old_producer.getDefiningOp()->erase();
      old_consumer.getDefiningOp()->erase();
    }

    // return a lambda to generate nodeinfo ptrs

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
