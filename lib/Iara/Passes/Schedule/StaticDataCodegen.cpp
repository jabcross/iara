#ifndef IARA_PASSES_SCHEDULE_STATICDATACODEGEN_H
#define IARA_PASSES_SCHEDULE_STATICDATACODEGEN_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/LowerToTasksPass.h"
#include "Iara/Util/Mlir.h"
#include <Iara/Dialect/IaraPasses.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>

using namespace iara;
using namespace iara::passes;

// Values to be copied to buffer on first allocation.
template <typename ValueType>
std::string codegenFirstBuffer(LowerToTasksPass *_this, NodeOp node) {

  assert(node.isAlloc());
  std::string rv;
  llvm::raw_string_ostream rvs{rv};

  std::string list;
  llvm::raw_string_ostream list_s{list};

  auto edge = followChainUntilNext<EdgeOp>(*node.getOut().begin());
  auto elem_type = getElementTypeOrSelf(edge.getIn().getType());

  int counter = 0;

  auto dealloc_node = _this->getMatchingDealloc(node);

  auto current_edge =
      followChainUntilPrevious<EdgeOp>(dealloc_node->getOperand(0));

  while (current_edge) {
    auto delay_attr = *current_edge["delay"];
    if (!delay_attr) {
      // noop
    } else if (auto array =
                   llvm::dyn_cast<mlir::detail::DenseArrayAttrImpl<ValueType>>(
                       delay_attr)) {
      auto arrref = array.asArrayRef();
      for (auto &val : arrref) {
        counter++;
        list_s << val;
        if (&val != &arrref.back()) {
          list_s << ", ";
        } else {
          list_s << " // Delay of edge " << (i64)current_edge["edge_id"]
                 << "\n      ";
        }
      }
    } else if (auto number = llvm::dyn_cast<mlir::IntegerAttr>(delay_attr)) {
      for (int i = 0, e = number.getInt(); i < e; i++) {
        counter++;
        list_s << ValueType(0);
        if (i < e - 1) {
          list_s << ", ";
        } else {
          list_s << " // Delay of edge " << (i64)current_edge["edge_id"]
                 << "\n      ";
        }
      }
    } else {
      llvm_unreachable("Unexpected delay attr type");
    }
    current_edge = followInoutChainBackwards(current_edge);
  }

  rvs << "(byte*)(" << getCTypeName(elem_type) << "[" << counter
      << "]){\n      ";
  rvs << list;
  rvs << "}";
  return rv;
}

std::string codegenFirstBuffer(LowerToTasksPass *_this, NodeOp node) {

  if (!node.isAlloc()) {
    return "nullptr";
  }

  auto edge = followChainUntilNext<EdgeOp>(*node.getOut().begin());
  auto elem_type = getElementTypeOrSelf(edge.getIn().getType());

  if (elem_type == mlir::IntegerType::get(node->getContext(), 64)) {
    return codegenFirstBuffer<i64>(_this, node);
  } else if (elem_type == mlir::IntegerType::get(node->getContext(), 32)) {
    return codegenFirstBuffer<int32_t>(_this, node);
  } else {
    llvm_unreachable("unimplemented");
  }
}

void LowerToTasksPass::codegenHeaderFile(mlir::OpBuilder &func_builder,
                                         llvm::StringRef path,
                                         llvm::SmallVector<NodeOp> &nodes,
                                         llvm::SmallVector<EdgeOp> &edges) {
  std::error_code ec;
  llvm::raw_fd_stream include{path, ec};
  if (ec) {
    llvm::errs() << "Could not open file " << path << "\n";
    return;
  }
  include << "// Generated automatically by IaRa. Editing manually may not "
             "be productive.\n";
  include << "#define IARA_RUNTIME\n";
  include << "#include <IaraRuntime/IaraRuntime.h>\n";
  include << "using namespace iara::runtime;\n";

  llvm::DenseMap<NodeOp, std::string> node_names;
  llvm::DenseMap<NodeOp, llvm::SmallVector<EdgeOp>> node_successors;

  i64 num_buffers = 0;

  for (auto node : nodes) {
    if (node.isDealloc()) {
      num_buffers += (i64)node["total_firings"];
    }
    for (auto output : node.getAllOutputs()) {
      auto successor = util::mlir::followChainUntilNext<EdgeOp>(output);
      assert(successor);
      node_successors[node].push_back(successor);
    }

    node_names[node] = getTaskFuncName(node, func_builder);
    if (node_names[node].empty())
      continue;
    include << "extern \"C\" void " << node_names[node] << "(i64, byte **);\n";
  }

  include << "const char*[] iara_runtime_delay_buffers = {\n";

  int delay_buffer_index = 0;
  DenseMap<NodeOp, i64> node_to_delay_index;
  for (auto node : nodes) {
    node_to_delay_index[node] = delay_buffer_index++;
    include << "  " << ::codegenFirstBuffer(this, node);
    if (node != nodes.back()) {
      include << ",";
    }
    include << "\n";
  }
  include << "},";

  include << "const StaticData iara_runtime_static_data = {\n";

  include << ".num_nodes = " << nodes.size() << ",\n";
  include << ".num_edges = " << edges.size() << ",\n";
  include << ".num_buffers = " << num_buffers << ",\n";

  include << ".nodes = (NodeData[]){\n";

  for (auto node : nodes) {
    include << "  NodeData{\n";
    include << "    .input_port_count = " << node.getAllInputs().size()
            << ",\n";
    include << "    .input_bytes = " << getTotalInputBytes(node) << ",\n";
    include << "    .output_port_count = " << node.getAllOutputs().size()
            << ",\n";
    include << "    .total_firings = "
            << node->getAttrOfType<IntegerAttr>("total_firings").getInt()
            << ",\n";
    include << "    .output_edges = (i64[]){";
    for (auto &s : node_successors[node]) {
      include << s->getAttrOfType<IntegerAttr>("edge_id").getInt();
      if (s != node_successors[node].back()) {
        include << ", ";
      }
    };
    include << "},\n";
    {
      include << "    .func_call = ";
      if (node.isDealloc() or node.isAlloc()) {
        include << "nullptr";
      } else {
        include << "&" << node_names[node];
      }
      include << ",\n";
    }

    // delays

    include << "    .init_buffer = iara_runtime_delay_buffers["
            << node_to_delay_index[node] << "]\n";

    include << "  }";
    if (node != nodes.back()) {
      include << ",";
    }
    include << "\n";
  }

  include << "},\n";

  include << ".edges = (EdgeData[]){\n";

  for (auto edge : edges) {

    include << "  EdgeData{\n";

    // auto edge_id = edge->getAttrOfType<IntegerAttr>("edge_id").getInt();
    auto cons_alpha = edge->getAttrOfType<IntegerAttr>("cons_alpha").getInt();
    auto cons_beta = edge->getAttrOfType<IntegerAttr>("cons_beta").getInt();

    include << "    .prod_rate = " << (i64)edge["prod_rate"]
            << ",\n"; // in bytes, -1 for alloc node
    include << "    .prod_alpha = " << (i64)edge["prod_alpha"] << ",\n";
    include << "    .prod_beta = " << (i64)edge["prod_beta"] << ",\n";
    include << "    .prod_id = " << (i64)edge.getProducerNode()["node_id"]
            << ",\n";
    include << "    .cons_rate = " << (i64)edge["cons_rate"]
            << ",\n"; // in bytes, -1 for dealloc node
    include << "    .cons_alpha = " << cons_alpha << ",\n";
    include << "    .cons_beta = " << cons_beta << ",\n";
    include << "    .cons_id = " << (i64)edge.getConsumerNode()["node_id"]
            << ",\n";
    include << "    .this_delay = " << (i64)edge["delay_bytes"] << ",\n";
    include << "    .remaining_delay = " << (i64)edge["remaining_delay"]
            << ",\n";
    include << "    .cons_input_port_index=  " << getConsPortIndex(edge)
            << ",\n";
    include << "    .buffer_size_with_delays=  "
            << (i64)edge["buffer_size_with_delays"] << ",\n";
    include << "    .buffer_size_without_delays =  "
            << (i64)edge["buffer_size_without_delays"] << "\n";
    include << "    .total_delay_size =  " << (i64)edge["total_delay_size"]
            << ",\n";
    auto alloc = getMatchingAlloc(edge.getConsumerNode());
    include << "    .delay_buffer =  iara_runtime_delay_buffers["
            << node_to_delay_index[alloc] << "],\n";
    include << "    .copyback_buffer =  (char[" << (i64)edge["total_delay_size"]
            << "]){0}\n";
    include << "  }";
    if (edge != edges.back()) {
      include << ",";
    }
    include << "\n";
  }
  include << "} };\n";

  include.flush();
}

#endif
