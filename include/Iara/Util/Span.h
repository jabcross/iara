#ifndef IARA_UTIL_SPAN_H
#define IARA_UTIL_SPAN_H

#include <span>
#include <vector>

template <class T> struct Span {
  T *ptr = nullptr;
  size_t extents = 0;

  operator std::span<T>() const { return std::span<T>{ptr, extents}; }

  static Span<T> from(const std::span<T> &span) {
    if (span.empty())
      return {};
    return {span.begin(), span.size()};
  }

  static Span<T> from(const std::vector<T> &vec) {
    if (vec.empty())
      return {};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
    return {(T *)&*vec.cbegin(), vec.size()};
#pragma clang diagnostic pop
  }

  inline size_t size() const { return extents; }

  inline std::span<T> asSpan() { return std::span<T>{ptr, extents}; }
};

#endif