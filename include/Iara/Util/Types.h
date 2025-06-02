#ifndef UTIL_TYPES_H
#define UTIL_TYPES_H

#include <cstdint>
#include <gtl/phmap.hpp>
#include <llvm/ADT/SmallVector.h>
#include <span>

using i64 = int64_t;
using std::byte;

template <class... T> using Vec = llvm::SmallVector<T...>;
template <class T> using PairOf = std::pair<T, T>;
template <class Key, class Value>
using ParallelHashMap =
    gtl::parallel_flat_hash_map<Key, Value, gtl::priv::hash_default_hash<Key>,
                                gtl::priv::hash_default_eq<Key>,
                                std::allocator<std::pair<const Key, Value>>, 4,
                                std::mutex>;

#endif // UTIL_TYPES_H
