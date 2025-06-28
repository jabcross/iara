#ifndef IARA_PASSES_VIRTUALFIFO_SDF_ANALYSIS_H
#define IARA_PASSES_VIRTUALFIFO_SDF_ANALYSIS_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/VirtualFIFO/SDF/SDF.h"

namespace iara::passes::virtualfifo::sdf {

// Uses ILP to calculate buffer sizes and graph feasibility, and annotates it.
LogicalResult analyseVirtualBufferSizes(ActorOp actor,
                                        StaticAnalysisData &data);

} // namespace iara::passes::virtualfifo::sdf

#endif
