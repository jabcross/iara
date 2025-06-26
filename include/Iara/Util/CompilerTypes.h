#ifndef IARA_UTIL_COMPILERTYPES_H
#define IARA_UTIL_COMPILERTYPES_H

#include <llvm/ADT/SmallVector.h>

template <class... T> using Vec = llvm::SmallVector<T...>;
template <class T> using PairOf = std::pair<T, T>;

#endif
