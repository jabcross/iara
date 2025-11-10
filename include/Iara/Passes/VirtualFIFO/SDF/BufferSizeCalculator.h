#ifndef IARA_PASSES_VIRTUALFIFO_SDF_BUFFERSIZECALCULATOR_H
#define IARA_PASSES_VIRTUALFIFO_SDF_BUFFERSIZECALCULATOR_H

#include "Iara/Util/CommonTypes.h"
#include "Iara/Util/CompilerTypes.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LogicalResult.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

namespace iara::passes::virtualfifo::sdf {

// Alpha and beta are calculated per actor, per inout
// buffer that passes
// through
// it, and represent the number of firings that access the buffer. Alpha is for
// the first buffer that contains the delays, beta is for the remaining buffers.
struct BufferSizeValues {
  Vec<i64> alpha;
  Vec<i64> beta; // Number of firings of a given node that access the buffer
};

struct BufferSizeMemo {

  std::unordered_map<llvm::hash_code, BufferSizeValues> memo;

  std::pair<bool, BufferSizeValues *> get(llvm::SmallVector<i64> &rates,
                                          llvm::SmallVector<i64> &delays);
};

llvm::FailureOr<BufferSizeValues *>
calculateBufferSize(BufferSizeMemo &memo,
                    llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays);

llvm::FailureOr<BufferSizeValues *>
calculateBufferSizePresburger(BufferSizeMemo &memo,
                               llvm::SmallVector<i64> &rates,
                               llvm::SmallVector<i64> &delays);
} // namespace iara::passes::virtualfifo::sdf

#endif