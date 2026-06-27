#ifndef IARARUNTIME_UTIL_DEBUGPRINT_H
#define IARARUNTIME_UTIL_DEBUGPRINT_H

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <thread>
#include <unistd.h>
#include <sys/syscall.h>

// Backend-agnostic OS kernel thread id: identical mechanism under the OMP and
// EnkiTS backends (both run on pthreads), so firings can be attributed to a
// concrete thread regardless of the scheduler. std::thread::id alone only
// gave a 7-bucket color; this prints the real tid plus a monotonic timestamp
// so per-firing concurrency/overlap is unambiguous in the log.
inline long iaraDebugTid() {
#if defined(SYS_gettid)
  return (long)syscall(SYS_gettid);
#else
  return (long)std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}

inline double iaraDebugTime() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

inline void debugPrintThreadColor(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  long tid = iaraDebugTid();
  auto offset = (unsigned long)tid % 7;
  fprintf(stderr, "\e[%2lum[tid=%ld t=%.6f] ", 31 + offset, tid,
          iaraDebugTime());
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\e[39m");
  fflush(stderr);

  va_end(args);
}

#endif