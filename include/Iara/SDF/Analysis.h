#ifndef IARA_SDF_ANALYSIS_H
#define IARA_SDF_ANALYSIS_H

#include "Iara/Dialect/IaraOps.h"
#include "Iara/SDF/SDF.h"

namespace iara::sdf {

// Uses ILP to calculate buffer sizes and graph feasibility, and annotates it.
LogicalResult analyseVirtualBufferSizes(ActorOp actor,
                                        iara::sdf::StaticAnalysisData &data);

} // namespace iara::sdf

#endif
