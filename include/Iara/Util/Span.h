#ifndef IARA_UTIL_SPAN_H
#define IARA_UTIL_SPAN_H

#include <span>
#include <vector>

template <class T> struct Span {
  T *ptr = nullptr;
  size_t extents = 0;

  operator std::span<T>() const { return std::span<T>{ptr, extents}; }

  static Span<T> from(std::span<T> span) {
    if (span.empty())
      return {};
    return {span.begin(), span.size()};
  }

  static Span<T> from(std::vector<T> &vec) {
    if (vec.empty())
      return {};
    return {&vec.front(), vec.size()};
  }

  inline size_t size() const { return extents; }
};

#endif