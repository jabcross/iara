#ifndef IARARUNTIME_MUTEXHASHMAP_H
#define IARARUNTIME_MUTEXHASHMAP_H

#include <functional>
#include <mutex>
#include <unordered_map>

// Simple, non-parallel implementation.

template <class Key, class Value> struct MutexHashMap {
  std::mutex lock;
  std::unordered_map<Key, Value> map;

  using constructor = std::function<void(Key, Value &&)>;
  using value_type = std::unordered_map<Key, Value>::value_type;

  using onFirstToArriveFunc = std::function<void(constructor &&)>;
  using onSecondOnwardsFunc = std::function<void(value_type &)>;

  std::unordered_map<int, int> x;
  std::unordered_map<int, int>::iterator i;

  void
  lazy_emplace_l(Key key, onSecondOnwardsFunc osof, onFirstToArriveFunc oftaf) {
    lock.lock();
    auto entry = map.find(key);
    if (entry == map.end()) {
      oftaf([&](Key key, Value &&value) { map[key] = std::move(value); });
    } else {
      osof(*entry);
    }
    lock.unlock();
  }
  void erase(Key key) {
    lock.lock();
    map.erase(key);
    lock.unlock();
  }
  bool contains(Key key) {
    lock.lock();
    bool found = map.find(key) != map.end();
    lock.unlock();
    return found;
  }
  bool empty() {
    lock.lock();
    bool e = map.empty();
    lock.unlock();
    return e;
  }
};

#endif