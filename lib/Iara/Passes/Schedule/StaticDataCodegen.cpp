#ifndef IARA_PASSES_SCHEDULE_STATICDATACODEGEN_H
#define IARA_PASSES_SCHEDULE_STATICDATACODEGEN_H

#include "Iara/Passes/LowerToTasksPass.h"
#include "Util/MlirUtil.h"
#include <Iara/IaraPasses.h>
#include <mlir/IR/Builders.h>

using namespace mlir::iara::passes;

void LowerToTasksPass::generateHeaderFile(mlir::OpBuilder &func_builder,
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

  for (auto node : nodes) {
    for (auto output : node.getAllOutputs()) {
      auto successor = mlir_util::followChainUntilNext<EdgeOp>(output);
      assert(successor);
      node_successors[node].push_back(successor);
    }

    node_names[node] = getTaskFuncName(node, func_builder);
    if (node_names[node].empty())
      continue;
    include << "extern \"C\" void " << node_names[node]
            << "(int64_t, uint8_t **);\n";
  }

  include << "const StaticData iara_runtime_static_data = {\n";

  include << ".num_nodes = " << nodes.size() << ",\n";
  include << ".num_edges = " << edges.size() << ",\n";

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
    include << "    .output_edges = (int64_t[]){";
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
        include << "nullptr\n";
      } else {
        include << "&" << node_names[node] << "\n";
      }
    }
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

    include << "    .prod_rate = " << (int64_t)edge["prod_rate"]
            << ",\n"; // in bytes, -1 for alloc node
    include << "    .prod_alpha = " << (int64_t)edge["prod_alpha"] << ",\n";
    include << "    .prod_beta = " << (int64_t)edge["prod_beta"] << ",\n";
    include << "    .prod_id = " << (int64_t)edge.getProducerNode()["node_id"]
            << ",\n";
    include << "    .cons_rate = " << (int64_t)edge["cons_rate"]
            << ",\n"; // in bytes, -1 for dealloc node
    include << "    .cons_alpha = " << cons_alpha << ",\n";
    include << "    .cons_beta = " << cons_beta << ",\n";
    include << "    .cons_id = " << (int64_t)edge.getConsumerNode()["node_id"]
            << ",\n";
    include << "    .this_delay = " << edge.getDelay() << ",\n";
    include << "    .remaining_delay = " << (int64_t)edge["remaining_delay"]
            << ",\n";
    include << "    .cons_input_port_index=  " << getConsPortIndex(edge)
            << ",\n";
    include << "    .buffer_size_with_delays=  "
            << (int64_t)edge["buffer_size_with_delays"] << ",\n";
    include << "    .buffer_size_without_delays =  "
            << (int64_t)edge["buffer_size_without_delays"] << ",\n";
    include << "    .init_buffer = " << codegenFirstBuffer(edge) << "\n";
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