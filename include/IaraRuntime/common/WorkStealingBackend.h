#ifndef IARA_RUNTIME_WORKSTEALINGBACKEND_H
#define IARA_RUNTIME_WORKSTEALINGBACKEND_H

// This header provides an abstraction layer for parallelism in VirtualFIFO runtime.
// It can be configured to use either OpenMP or EnkiTS through macro definitions.

// =============================================================================
// Configuration: Select parallelism backend
// =============================================================================
// Define one of:
//   IARA_PARALLELISM_OMP    - Use OpenMP backend
//   IARA_PARALLELISM_ENKITS - Use EnkiTS backend
//   IARA_PARALLELISM_NONE   - Sequential execution (no parallelism)

#if !defined(IARA_PARALLELISM_OMP) && !defined(IARA_PARALLELISM_ENKITS) && !defined(IARA_PARALLELISM_NONE)
  // Default to OpenMP if nothing is specified
  #define IARA_PARALLELISM_OMP
#endif

// =============================================================================
// Feature detection
// =============================================================================
#if defined(IARA_PARALLELISM_OMP) || defined(IARA_PARALLELISM_ENKITS)
  #define IARA_HAS_PARALLELISM 1
#else
  #define IARA_HAS_PARALLELISM 0
#endif

// =============================================================================
// Include backend-specific implementation
// =============================================================================
#ifdef IARA_PARALLELISM_OMP
  #include "WorkStealingBackend_OMP.h"
#elif defined(IARA_PARALLELISM_ENKITS)
  #include "WorkStealingBackend_EnkiTS.h"
#elif defined(IARA_PARALLELISM_NONE)
  #include "WorkStealingBackend_Sequential.h"
#endif

#endif // IARA_RUNTIME_WORKSTEALINGBACKEND_H
