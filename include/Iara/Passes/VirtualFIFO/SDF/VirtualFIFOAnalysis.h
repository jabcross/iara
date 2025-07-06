#ifndef IARA_PASSES_VIRTUALFIFO_SDF_VIRTUALFIFOANALYSIS_H
#define IARA_PASSES_VIRTUALFIFO_SDF_VIRTUALFIFOANALYSIS_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"

namespace iara::passes::virtualfifo::sdf {

// Analyzes the edges in this chain following virtual fifo rules. If succeeded,
// fills out the static analysis data for the edges in the chain.

LogicalResult analyzeVirtualInoutChain(Vec<iara::EdgeOp> &chain,
                                       StaticAnalysisData &data);
} // namespace iara::passes::virtualfifo::sdf

#endif