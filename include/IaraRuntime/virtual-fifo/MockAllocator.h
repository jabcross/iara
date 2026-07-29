#ifndef IARARUNTIME_VIRTUALFIFO_MOCKALLOCATOR_H
#define IARARUNTIME_VIRTUALFIFO_MOCKALLOCATOR_H

// Profiling-only mock allocator (enable with -DIARA_MOCK_ALLOC).
//
// Establishes the zero-cost-allocation UPPER BOUND on runtime performance.
// Every allocation larger than IARA_MOCK_THRESHOLD — the bulk float
// pyramid/image buffers that today get a private malloc per broadcast copy —
// is aliased onto ONE shared, lazily mmap'd region. Consequences:
//   * no per-buffer malloc, no copies materialized (all big buffers overlap),
//   * pages faulted at most once and then reused across every firing and every
//     parallel worker => RSS and minor-fault count go flat with core count,
//   * results are GARBAGE (overlapping writes). Use ONLY for timing/RSS/fault
//     profiling, never for correctness.
//
// Allocations <= IARA_MOCK_THRESHOLD fall back to real malloc. Those are the
// tiny iterator/counter/column_sizes/keypoint CONTROL buffers whose CONTENTS
// drive loop bounds and indices; keeping them real and correct is what stops
// the kernels from running off the end of the shared garbage region. (See the
// break-risk analysis: only sub-MB integer/struct edges carry control data;
// every >1MB buffer is pure pixel data safe to alias.)

#include <cstdint>
#include <cstdlib>

#ifdef IARA_MOCK_ALLOC

#include <mutex>
#include <sys/mman.h>

#ifndef IARA_MOCK_THRESHOLD
#define IARA_MOCK_THRESHOLD (1ull << 20) // 1 MB
#endif

#ifndef IARA_MOCK_RESERVE
// Virtual-only reservation (MAP_NORESERVE); no RSS until touched. Must exceed
// the largest single block (~4 GB at 4K). 24 GB leaves headroom.
#define IARA_MOCK_RESERVE (24ull << 30)
#endif

inline unsigned char *iara_mock_region() {
  static unsigned char *region = nullptr;
  static std::once_flag once;
  std::call_once(once, [] {
    void *p = mmap(nullptr, IARA_MOCK_RESERVE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    region = (p == MAP_FAILED) ? (unsigned char *)malloc(IARA_MOCK_RESERVE)
                               : (unsigned char *)p;
  });
  return region;
}

inline bool iara_mock_owns(const void *p) {
  unsigned char *r = iara_mock_region();
  return (const unsigned char *)p >= r &&
         (const unsigned char *)p < r + IARA_MOCK_RESERVE;
}

// All big buffers alias the same region base (max overlap => constant RSS).
inline void *iara_mock_alloc(int64_t size) {
  if ((uint64_t)size > (uint64_t)IARA_MOCK_THRESHOLD)
    return iara_mock_region();
  return malloc((size_t)size);
}

inline void iara_mock_free(void *p) {
  if (!iara_mock_owns(p))
    free(p);
}

inline void *iara_malloc(int64_t size) { return iara_mock_alloc(size); }
inline void iara_runtime_free(void *p) { iara_mock_free(p); }

#else // !IARA_MOCK_ALLOC

inline void *iara_malloc(int64_t size) { return malloc((size_t)size); }
inline void iara_runtime_free(void *p) { free(p); }

#endif // IARA_MOCK_ALLOC
#endif // IARARUNTIME_VIRTUALFIFO_MOCKALLOCATOR_H
