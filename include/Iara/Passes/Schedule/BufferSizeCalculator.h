

#include "Util/RangeUtil.h"
#include <cstdint>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LogicalResult.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

llvm::FailureOr<
    std::pair<llvm::SmallVector<int64_t>, llvm::SmallVector<int64_t>>>
calculateBufferSize(llvm::SmallVector<int64_t> &rates,
                    llvm::SmallVector<int64_t> &delays);