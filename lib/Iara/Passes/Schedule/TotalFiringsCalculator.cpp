#include "Iara/IaraOps.h"
#include "Iara/Passes/LowerToTasksPass.h"
#include "Util/MlirUtil.h"
#include "Util/RangeUtil.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Support/LLVM.h>

using namespace mlir::iara;
using namespace mlir::iara::passes;

// Calculate total firings with another ILP model.
mlir::LogicalResult LowerToTasksPass::calculateTotalFirings(ActorOp actor) {

  auto input_filename = "total_firings.input.txt";
  auto output_filename = "total_firings.output.txt";

  // Generate input file.

  std::error_code input_error_code;
  llvm::raw_fd_ostream input_file{input_filename, input_error_code,
                                  llvm::sys::fs::FileAccess::FA_Write};

  auto nodes = actor.getOps<NodeOp>() | IntoVector();

  DenseMap<i64, NodeOp> nodeById{};

  Vec<NodeOp> alloc_nodes;

  input_file << nodes.size() << "\n";
  for (auto node : nodes) {
    nodeById[(i64)node["node_id"]] = node;
    input_file << (i64)node["node_id"] << " ";
    if (node.isAlloc()) {
      alloc_nodes.push_back(node);
    }
    if (node.isAlloc() or node.isDealloc()) {
      input_file << "1 1\n";
    } else {
      for (auto input : node.getAllInputs()) {
        auto edge = followChainUntilPrevious<EdgeOp>(input);
        input_file << (i64)edge["cons_alpha"] << " " << (i64)edge["cons_beta"]
                   << " ";
      }
      input_file << "\n";
    }
  }
  input_file << alloc_nodes.size() << "\n";
  for (auto node : alloc_nodes) {
    input_file << (i64)node["node_id"] << " ";
    auto edge = followChainUntilNext<EdgeOp>(node->getResult(0));
    while (edge != nullptr) {
      input_file << (i64)edge.getConsumerNode()["node_id"] << " ";
      edge = followInoutEdgeForwards(edge);
    }
    input_file << "\n";
  }

  input_file.close();

  // Run calculator.

  std::string command;
  llvm::raw_string_ostream ss(command);
  auto iara_dir = std::getenv("IARA_DIR");

  if (iara_dir == NULL or strnlen(iara_dir, 256) == 0) {
    llvm::errs()
        << "ERROR: Empty environment variable IARA_DIR. Did you source the "
           ".env file? \n";
    return failure();
  };

  llvm::errs() << "Value of IARA_DIR: >" << iara_dir << "< ";

  ss << std::getenv("IARA_DIR") << "/scripts/pyenv/bin/python "
     << std::getenv("IARA_DIR") << "/scripts/total-firings-calculator.py "
     << input_filename;

  llvm::SmallString<128> errpath;
  auto error_code = llvm::sys::fs::createTemporaryFile("", ".txt", errpath);

  if (error_code.value() != 0) {
    llvm::errs() << "Error creating temporary file: " << errpath << "\n";
    return failure();
  }

  ss << " 2> " << errpath;

  llvm::errs() << "Running command: " << command << "\n";

  auto return_code = std::system(command.c_str());

  llvm::errs() << "Exit code: " << llvm::formatv("{0}", return_code) << "\n";

  if (return_code != 0) {
    llvm::errs() << "Error in total firings calculation.";
    return failure();
  }

  auto output_file = llvm::MemoryBuffer::getFileAsStream(output_filename)
                         ->get()
                         ->getBuffer()
                         .str();

  Vec<StringRef> lines{};
  for (auto line : llvm::split(output_file, "\n")) {
    if (line.size() > 0)
      lines.push_back(line);
  }

  assert(lines.size() == nodes.size());

  for (auto [node, line] : llvm::zip_equal(nodes, lines)) {
    auto [id_str, total_firings_str] = line.split(" ");
    i64 id, total_firings;
    if (id_str.consumeInteger(10, id) or
        total_firings_str.consumeInteger(10, total_firings)) {
      llvm::errs() << "Failure consuming integers from string <" << line
                   << ">\n";
      return failure();
    };
    nodeById[id]["total_firings"] = total_firings;
  }
  return success();
}
