#ifndef IARARUNTIME_UTIL_DEBUGPRINT_H
#define IARARUNTIME_UTIL_DEBUGPRINT_H

#include <cstdarg>
#include <cstdio>
#include <thread>

inline void debugPrintThreadColor(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  auto offset = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 7;
  fprintf(stderr, "\e[%2lum", 31 + offset);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\e[39m");
  fflush(stderr);
}

#endif