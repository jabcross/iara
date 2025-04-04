#ifndef UTIL_TYPES_H
#define UTIL_TYPES_H

#include <cstdint>
#include <llvm/ADT/SmallVector.h>

using i64 = int64_t;
using std::byte;

template <class... T> using Vec = llvm::SmallVector<T...>;
template <class T> using PairOf = std::pair<T, T>;

#endif // UTIL_TYPES_H