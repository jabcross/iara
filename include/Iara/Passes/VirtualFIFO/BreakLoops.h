#ifndef IARA_PASSES_VIRTUALFIFO_BREAKLOOPS_H
#define IARA_PASSES_VIRTUALFIFO_BREAKLOOPS_H

#include "Iara/Dialect/IaraOps.h"

namespace iara::passes::virtualfifo {

void breakLoops(iara::dialect::ActorOp actor);

}

#endif