#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <cstddef>
#include <iterator>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/Casting.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/Operation.h>
#include <optional>
#include <type_traits>
#include <utility>

namespace RangeUtil {

template <class T, typename Enable = void> struct NullTypeOf {
  using type = std::optional<T>;
  static constexpr std::optional<T> value() { return std::nullopt; };
};

template <class T>
struct NullTypeOf<T, typename std::enable_if<std::is_convertible<
                         T, mlir::Operation *>::value>::type> {
  using type = T;
  static constexpr T value() { return nullptr; };
};

template <class F> struct Filter {
  using type = F;
  F f;
  Filter(F &&f) : f(f) {};
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

template <class C = std::nullptr_t> struct Into {};

template <class R> struct OwnedElementType {
  using type = decltype(*std::begin(std::declval<R>()));
};

template <
    unsigned Size, typename R,
    template <typename element_type, unsigned> typename CT = llvm::SmallVector>
CT<typename OwnedElementType<R>::type, Size>
operator|(R &&Range, Into<std::nullptr_t> &&) {
  return {std::begin(Range), std::end(Range)};
};

template <class R, class C> auto operator|(R &&range, Into<C> c) {
  return C{range};
}

// Converts a range of references to a range of pointers.
struct Pointers {};

template <class R> auto operator|(R &&range, Pointers) -> auto {
  return range | Map([](auto &x) { return &x; });
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

template <class F> struct Find {
  using type = F;
  F f;
  Find(F &&f) : f(f) {};
};

template <typename F> Find(F) -> Find<F>;
template <class R, class F>
auto operator|(R &&range, Find<F> &&find)
    -> NullTypeOf<decltype(*std::begin(range))>::type {
  for (auto i : range) {
    if (find.f(i))
      return {i};
  }
  return NullTypeOf<decltype(*std::begin(range))>::value();
}

struct Count {};

template <class R> auto operator|(R &&range, Count count) -> size_t {
  size_t rv = 0;
  for (auto i : range) {
    rv++;
  }
  return rv;
}

// allow piping into anything that takes begin and end iterators
template <class R, class F>
auto operator|(R &&range, F &&f)
    -> decltype(f(std::begin(range), std::end(range))) {
  return f(std::begin(range), std::end(range));
}

// Calls copy assignment on the object and returns the result.
struct Copy {};

auto operator|(auto &c, Copy) -> auto {
  auto rv = c;
  return rv;
}

// based on
// https://stackoverflow.com/questions/17805969/writing-universal-memoization-function-in-c11
template <typename... Args, typename F,
          typename R = std::invoke_result_t<F, Args...>>
auto memoize(F fn) {
  llvm::DenseMap<std::tuple<Args...>, R> table;
  return [fn, table](Args... args) mutable -> R {
    auto argt = std::make_tuple(args...);
    auto memoized = table.find(argt);
    if (memoized == table.end()) {
      memoized = table.insert({argt, fn(args...)}).first;
    }
    return memoized->second;
  };
}
} // namespace RangeUtil

#endif