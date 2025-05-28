
#include "mlir/IR/Operation.h"

using namespace mlir;

std::string printMermaid(Operation *op) {
  if (op->getNumRegions() == 0) {
    return "";
  }
}