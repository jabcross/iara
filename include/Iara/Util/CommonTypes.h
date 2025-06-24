#ifndef UTIL_TYPES_H
#define UTIL_TYPES_H

// Some types. Safe to be included by both the compiler and the runtime.

#include <cstdint>
#include <gtl/phmap.hpp>
#include <llvm/ADT/SmallVector.h>

using i8 = int8_t;
using u8 = uint8_t;

using i32 = int32_t;
using u32 = uint64_t;

using i64 = int64_t;
using u64 = uint64_t;

template <class... T> using Vec = llvm::SmallVector<T...>;
template <class T> using PairOf = std::pair<T, T>;
template <class Key, class Value>
using ParallelHashMap =
    gtl::parallel_flat_hash_map<Key,
                                Value,
                                gtl::priv::hash_default_hash<Key>,
                                gtl::priv::hash_default_eq<Key>,
                                std::allocator<std::pair<const Key, Value>>,
                                4,
                                std::mutex>;

#endif // UTIL_TYPES_H
