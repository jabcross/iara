#ifndef IARA_UTIL_FOREACHTYPE_H
#define IARA_UTIL_FOREACHTYPE_H
#include <cstddef>
#include <tuple>

namespace iara::util::foreachtype {

template <size_t i> struct Index {
  const static size_t val = i;
  constexpr inline size_t getSizeT() { return i; }
};

template <class T> struct TypeWrapper {
  using type = T;
};

template <size_t i, class H, class... Ts> struct ForEachType {
  inline static void iterate(auto f) {
    f(TypeWrapper<H>{}, Index<i>{});
    ForEachType<i + 1, Ts...>::iterate(f);
  }
};

template <size_t i, class H> struct ForEachType<i, H> {
  inline static void iterate(auto f) { f(TypeWrapper<H>{}, Index<i>{}); }
};

template <class... Ts> void for_each_type(auto f) {
  ForEachType<0, Ts...>::iterate(f);
}

template <class... Ts>
void for_each_tuple_type(TypeWrapper<std::tuple<Ts...>>, auto f) {
  ForEachType<0, Ts...>::iterate(f);
}

} // namespace iara::util::foreachtype
#endif