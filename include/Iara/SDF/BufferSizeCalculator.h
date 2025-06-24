#include "Iara/Util/CommonTypes.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LogicalResult.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

namespace iara::sdf {

// Alpha and beta are calculated per actor, per inout
// buffer that passes
// through
// it, and represent the number of firings that access the buffer. Alpha is for
// the first buffer that contains the delays, beta is for the remaining buffers.
struct BufferSizeValues {
  Vec<i64> alpha;
  Vec<i64> beta; // Number of firings of a given node that access the buffer
};

llvm::FailureOr<BufferSizeValues>
calculateBufferSize(llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays);
} // namespace iara::sdf
