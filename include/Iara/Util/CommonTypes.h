#ifndef IARA_UTIL_COMMONTYPES_H
#define IARA_UTIL_COMMONTYPES_H

// Some types. Safe to be included by both the compiler and the runtime.

#include <cstdint>

using i8 = int8_t;
using u8 = uint8_t;

using i32 = int32_t;
using u32 = uint64_t;

using i64 = int64_t;
using u64 = uint64_t;

// Special node type markers (encoded in VirtualFIFO_Node_StaticInfo::arg_bytes)
enum class NodeType : i64 {
  Normal = 0,      // Regular compute node (arg_bytes >= 0)
  Alloc = -2,      // Memory allocation node
  Dealloc = -3,    // Memory deallocation node
  Scatter = -4,    // Data scattering/partitioning node
  Gather = -5,     // Data gathering/concatenation node
};

#endif // UTIL_TYPES_H
