#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <iterator>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/Casting.h>
#include <type_traits>
#include <utility>
template <class F> struct Filter {
  using type = F;
  F f;
  Filter(F &&f) : f(f){};
};
template <typename F> Filter(F) -> Filter<F>;
template <class R, class F>
auto operator|(R &&range, Filter<F> &&transform) -> auto {
  return llvm::make_filter_range(std::forward<R>(range),
                                 std::forward<F>(transform.f));
}

template <class F> struct Map {
  using type = F;
  F f;
  Map(F &&f) : f(f) {}
};
template <typename F> Map(F) -> Map<F>;

template <class R, class F, class T = decltype(std::begin(std::declval<R>()))>
auto operator|(R &&range, Map<F> &&transform) -> auto {
  return llvm::map_range(std::forward<R>(range), std::forward<F>(transform.f));
}

template <class T> struct OfType {
  using type = T;
};

template <class R, class T> auto operator|(R &&range, OfType<T> &&) -> auto {
  return range | Filter([](auto &x) { return llvm::isa<T>(x); }) |
         Map([](auto &x) { return llvm::cast<T>(x); });
}

template <class C> struct Into {};

template <class R, class C> auto operator|(R &&range, Into<C> c) {
  return C{range};
}

struct Drop {
  unsigned int count;
  Drop(unsigned int count) : count(count) {}
};

template <class R> auto operator|(R &&range, Drop drop) -> auto {
  return llvm::drop_begin(range, drop.count);
}

// treat optional ranges as ranges
template <class R, class S>
auto operator|(std::optional<R> &&range, S &&stage)
    -> decltype(std::declval<R>() | std::forward<S>(stage)) {
  auto begin = decltype(std::begin(*range))();
  auto end = decltype(std::begin(*range))();
  if (range) {
    begin = std::begin(*range);
    end = std::end(*range);
  }
  return llvm::make_range(begin, end) | std::forward<S>(stage);
}

// allow piping into anything that takes begin and end iterators
template <class R, class F>
auto operator|(R &&range, F &&f)
    -> decltype(f(std::begin(range), std::end(range))) {
  return f(std::begin(range), std::end(range));
}

#endif
