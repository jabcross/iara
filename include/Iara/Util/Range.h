#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include "CommonTypes.h"
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallVectorExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/Casting.h>
// #include <mlir/Dialect/Func/IR/FuncOps.h>
// #include <mlir/IR/Attributes.h>
// #include <mlir/IR/Builders.h>
// #include <mlir/IR/BuiltinAttributes.h>
// #include <mlir/IR/Dialect.h>
// #include <mlir/IR/Location.h>
// #include <mlir/IR/Operation.h>
#include <numeric>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace iara::util::range {
using namespace std::placeholders;

// template <class A, class B, class R = void> struct Piper {};

// template <class A, class B>
// struct Piper<A, B,
//              std::void_t<decltype(std::bind(std::declval<B>(),
//                                             std::declval<A>(), _1))>> {
// public:
//   static constexpr auto apply(A a, B b) { return std::bind(b, a, _1); }
// };

// template <class A, class B> auto constexpr apply(A a, B b) {
//   return Piper<A, B>::apply(std::forward<A>(a), std::forward<B>(b));
// }

// template <class A, class B> auto constexpr pipe(A a, B b) {
//   return Piper<A, B>::apply(std::forward<A>(a), std::forward<B>(b));
// }

template <class R> struct OwnedElementType {
  using type = decltype(*std::begin(std::declval<R>()));
};

// template <class T, typename Enable = void> struct NullTypeOf {
//   using type = std::optional<T>;
//   static constexpr std::optional<T> value() { return std::nullopt; };
// };

// template <class T>
// struct NullTypeOf<T, typename std::enable_if<std::is_convertible<
//                          T, mlir::Operation *>::value>::type> {
//   using type = T;
//   static constexpr T value() { return nullptr; };
// };

template <class F> struct Filter {
  using type = F;
  F f;
  Filter(F &&f) : f(f){};
};
template <typename F> Filter(F) -> Filter<F>;

template <class R, class F>
auto pipe(R &&range, Filter<F> &&transform) -> auto {
  return llvm::make_filter_range(std::forward<R>(range),
                                 std::forward<F>(transform.f));
}

// Uses a member function as a map, returns range of result values
template <typename Base, typename Ret, bool is_const = false, typename... Args>
struct MapMember {
  using Pointer = Ret (Base::*)(Args...);
  Pointer pointer;
  MapMember(Pointer &&p) : pointer(p) {}
};

template <typename Base, typename Ret, typename... Args>
struct MapMember<Base, Ret, true, Args...> {
  using Pointer = Ret (Base::*)(Args...) const;
  Pointer pointer;
  MapMember(Pointer &&p) : pointer(p) {}
};

template <typename Base, typename Ret, typename... Args>
MapMember(Ret (Base::*)(Args...)) -> MapMember<Base, Ret, false, Args...>;

template <typename Base, typename Ret, typename... Args>
MapMember(Ret (Base::*)(Args...) const) -> MapMember<Base, Ret, true, Args...>;

template <typename R,
          typename Base,
          typename Ret,
          bool is_const,
          typename... Args,
          class T = decltype(std::begin(std::declval<R>()))>
auto pipe(R &&range, MapMember<Base, Ret, is_const, Args...> &&transform)
    -> auto {
  return llvm::map_range(std::forward<R>(range),
                         std::move(std::bind(transform.pointer, _1)));
}

template <typename F> struct Map {
  F f;
  Map(F &&f) : f(f) {}
};

template <typename F> Map(F) -> Map<F>;

template <class R, class F, class T = decltype(std::begin(std::declval<R>()))>
auto pipe(R &&range, Map<F> &&transform) -> auto {
  return llvm::map_range(std::forward<R>(range), std::move(transform.f));
}

template <class F> struct Reduce {
  using type = F;
  F f;
  Reduce(F &&f) : f(f) {}
};
template <typename F> Reduce(F) -> Reduce<F>;

template <class R, class T = decltype(std::begin(std::declval<R>())), class F>
auto pipe(R &&range, Reduce<F> &&reduce) -> auto {
  return std::accumulate(begin(range), end(range), T(), reduce.f);
}

struct Sum {};

template <class R, class T = decltype(std::begin(std::declval<R>())), class F>
auto pipe(R &&range, Sum &&) -> auto {
  return range | Reduce(std::plus<T>());
}

template <class T> struct OfType {
  using type = T;
};

template <class R, class T> auto pipe(R &&range, OfType<T> &&) -> auto {
  return range | Filter([](auto &x) { return llvm::isa<T>(x); }) |
         Map([](auto &x) { return llvm::cast<T>(x); });
}

template <class C = std::nullptr_t> struct Into {};

// Wraps llvm::to_vector
struct IntoVector {};

template <class R> auto pipe(R &&range, IntoVector) -> auto {
  return llvm::to_vector(range);
}

template <unsigned Size,
          typename R,
          template <typename element_type, unsigned>
          typename CT = llvm::SmallVector>
CT<typename OwnedElementType<R>::type, Size> pipe(R &&Range,
                                                  Into<std::nullptr_t> &&) {
  return {std::begin(Range), std::end(Range)};
};

template <class R, class C> auto pipe(R &&range, Into<C> c) { return C{range}; }

// Converts a range of references to a range of pointers.
struct Pointers {};

template <class R> auto pipe(R &&range, Pointers) -> auto {
  return range | Map([](auto &x) { return &x; });
}

struct Drop {
  unsigned int count;
  Drop(unsigned int count) : count(count) {}
};

template <class R> auto pipe(R &&range, Drop drop) -> auto {
  return llvm::drop_begin(range, drop.count);
}

struct Take {
  unsigned int count;
  Take(unsigned int count) : count(count) {}
};

template <class R> auto pipe(R &&range, Take take) -> auto {
  return range.take_front(take.count);
}

// treat optional ranges as ranges
template <class R, class S>
auto pipe(std::optional<R> &&range, S &&stage)
    -> decltype(std::declval<R>() | std::forward<S>(stage)) {
  auto begin = decltype(std::begin(*range))();
  auto end = decltype(std::begin(*range))();
  if (range) {
    begin = std::begin(*range);
    end = std::end(*range);
  }
  return llvm::make_range(begin, end) | std::forward<S>(stage);
}

// template <class F> struct Find {
//   using type = F;
//   F f;
//   Find(F &&f) : f(f){};
// };

// template <typename F> Find(F) -> Find<F>;
// template <class R, class F>
// auto pipe(R &&range, Find<F> &&find)
//     -> NullTypeOf<decltype(*std::begin(range))>::type {
//   for (auto i : range) {
//     if (find.f(i))
//       return {i};
//   }
//   return NullTypeOf<decltype(*std::begin(range))>::value();
// }

struct Count {};

template <class R> auto pipe(R &&range, Count count) -> i64 {
  i64 rv = 0;
  for (auto _i : range) {
    std::ignore = _i;
    rv++;
  }
  return rv;
}

// allow piping into anything that takes begin and end iterators
template <class R, class F>
auto pipe(R &&range, F &&f) -> decltype(f(std::begin(range), std::end(range))) {
  return f(std::begin(range), std::end(range));
}

template <class F, class... Args> struct Wrapper {
  auto wrap(F &&f, Args &&...args) { return std::move(f(args...)); }
};

// If function has overloads, wrap in this
#define WRAP(f, ...) [](auto &&x) { return f(std::move(x)); }

// Calls copy assignment on the object and returns the result.
struct Copy {};

template <class T> auto pipe(T &c, Copy) -> auto {
  auto rv = c;
  return rv;
}

// based on
// https://stackoverflow.com/questions/17805969/writing-universal-memoization-function-in-c11
template <typename... Args,
          typename F,
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

template <class T> using StaticRange = std::initializer_list<T *>;

template <class A, class B> inline auto operator|(A &&a, B &&b) {
  return pipe(std::forward<A>(a), std::forward<B>(b));
}

template <class A, class B> inline auto operator->*(A &&a, B &&b) {
  return pipe(std::forward<A>(a), std::forward<B>(b));
}

} // namespace iara::util::range

// Allow optional ranges to be used in range-based for loops.
namespace std {

template <class _Container>
auto begin(std::optional<_Container> &opt) -> typename _Container::iterator {
  if (opt)
    return std::begin(opt.value());
  else
    return {};
}

template <class _Container>
auto end(std::optional<_Container> &opt) -> typename _Container::iterator {
  if (opt)
    return std::end(opt.value());
  else
    return {};
}

template <class _Container>
auto cbegin(std::optional<_Container> &opt) ->
    typename _Container::const_iterator {
  if (opt)
    return std::cbegin(opt.value());
  else
    return {};
}

template <class _Container>
auto cend(std::optional<_Container> &opt) ->
    typename _Container::const_iterator {
  if (opt)
    return std::cend(opt.value());
  else
    return {};
}

} // namespace std

#endif
