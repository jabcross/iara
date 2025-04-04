

#include "Util/RangeUtil.h"
#include <cstdint>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/LogicalResult.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

llvm::FailureOr<std::pair<llvm::SmallVector<i64>, llvm::SmallVector<i64>>>
calculateBufferSize(llvm::SmallVector<i64> &rates,
                    llvm::SmallVector<i64> &delays);