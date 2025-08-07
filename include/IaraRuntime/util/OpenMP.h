#ifndef IARARUNTIME_UTIL_OPENMP_H
#define IARARUNTIME_UTIL_OPENMP_H

#include <cstddef>
#include <future>
#include <omp.h>
#include <semaphore>
#include <thread>
template <class F> inline void callBlockingFunctionFromOpenMP(F &&f) {
  // assuming this is called from OpenMP

  omp_event_handle_t event{};
  int x;
#ifndef IARA_DISABLE_OMP
  #pragma omp taskgroup
#endif

  {
#ifndef IARA_DISABLE_OMP
  #pragma omp task detach(event) depend(out : x) untied
#endif

    {
      std::async([f = std::forward<F>(f), event]() {
        f();
#ifndef IARA_DISABLE_OMP
  #pragma omp task firstprivate(event)
#endif
        omp_fulfill_event(event);
      }).detach();
    }
  }
  return;
}

#ifndef IARA_USING_OPENMP_TASK
  #define IARA_USING_OPENMP_TASK
#endif

#ifdef IARA_USING_OPENMP_TASK

template <class Sema> struct IaraSemaphore {
  Sema sema;

  constexpr IaraSemaphore() : sema(0) {};
  constexpr explicit IaraSemaphore(auto count) : sema(count) {};

  inline void acquire() {
    callBlockingFunctionFromOpenMP([&sema = sema]() { sema.acquire(); });
  }

  inline void release(ptrdiff_t update = 1) {
    callBlockingFunctionFromOpenMP(
        [&sema = sema, update]() { sema.release(update); });
  }
};

#else

struct IaraSemaphore {
  std::counting_semaphore<> sema;
};

#endif

#endif
