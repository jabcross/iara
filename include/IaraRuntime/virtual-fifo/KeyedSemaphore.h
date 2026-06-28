#ifndef IARA_RUNTIME_FIRST_COMPLETER_TASK_H
#define IARA_RUNTIME_FIRST_COMPLETER_TASK_H

#include "Iara/Util/CommonTypes.h"
#include "IaraRuntime/virtual-fifo/MutexHashMap.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <gtl/phmap.hpp>
#include <iostream>
#include <memory>

namespace keyed_semaphore {

#ifdef IARA_USE_MUTEX_HASHMAP
template <class Key, class Value>
using ParallelHashMap = MutexHashMap<Key, Value>;

#else
template <class Key, class Value>
using ParallelHashMap =
    gtl::parallel_flat_hash_map<Key,
                                Value,
                                gtl::priv::hash_default_hash<Key>,
                                gtl::priv::hash_default_eq<Key>,
                                std::allocator<std::pair<const Key, Value>>,
                                4,
                                std::mutex>;
#endif

// A semaphore to manage dependencies, delegating the creation of the task to
// the first dependency to arrive.
template <template <class Key, class Value> class HashMap,
          class Data,
          class FirstArgs,
          class EveryTimeArgs,
          class LastArgs,
          void first_time_func(FirstArgs &, Data &),
          void every_time_func(EveryTimeArgs &, Data &),
          void last_time_func(LastArgs &, Data &)>

struct KeyedSemaphore {
  struct Entry {
    i64 remaining_resources;
    Data data;
  };

  HashMap<i64, Entry> map{};

  // The first dependency to access this key will execute `first_time_func`. The
  // last one will execute `last_time_func`. Each dependency will decrement the
  // counter. All callers are expected to supply the same value of
  // `total_resources`.
  void arrive(i64 key,
              i64 this_resources,
              i64 total_resources,
              FirstArgs &first_args,
              EveryTimeArgs &every_time_args,
              LastArgs &last_args) {

    assert(total_resources >= this_resources &&
           "Asking for more resources than total");

    if (this_resources == total_resources) {

      // debug
      // fprintf(stderr,
      //         "Trivial semaphore at %lu\n"
      //         "               id = %ld\n"
      //         "   this_resources = %ld\n"
      //         "  total_resources = %ld\n",
      //         (size_t)this,
      //         id,
      //         this_resources,
      //         total_resources);
      // fflush(stderr);

      // skip the semaphore
      Data data{};
      first_time_func(first_args, data);
      every_time_func(every_time_args, data);
      last_time_func(last_args, data);
      return;
    }

    assert(total_resources > this_resources);

    bool erase = false;

    auto onFirstToArrive = [key,
                            &first_args,
                            &every_time_args,
                            _this = this,
                            this_resources,
                            total_resources,
                            &erase,
                            _first_time_func = first_time_func,
                            _every_time_func = every_time_func](
                               decltype(map)::constructor &&ctor) {
      // fprintf(stderr,
      //         "First semaphore trigger at %lu\n"
      //         "               id = %ld\n"
      //         "   this_resources = %ld\n"
      //         "  total_resources = %ld\n",
      //         (size_t)_this,
      //         id,
      //         this_resources,
      //         total_resources);
      // fflush(stderr);

      Entry new_value{.remaining_resources = total_resources - this_resources,
                      .data = {}};
      assert(new_value.remaining_resources > 0);
      _first_time_func(first_args, new_value.data);
      _every_time_func(every_time_args, new_value.data);
      ctor(key, std::move(new_value));
    };

    auto onSecondOnwards = [key,
                            total_resources,
                            &every_time_args,
                            &last_args,
                            _this = this,
                            this_resources,
                            _every_time_func = every_time_func,
                            _last_time_func = last_time_func,
                            &erase](decltype(map)::value_type &iter) {
      // fprintf(stderr,
      //         "Subsequent semaphore trigger at %lu\n"
      //         "               id = %ld\n"
      //         "   this_resources = %ld\n"
      //         "  total_resources = %ld\n",
      //         (size_t)_this,
      //         id,
      //         this_resources,
      //         total_resources);
      // fflush(stderr);
      auto &entry = iter.second;
      entry.remaining_resources -= this_resources;
      assert(entry.remaining_resources >= 0 &&
             "Asking for more resources than available");
      _every_time_func(every_time_args, entry.data);
      if (entry.remaining_resources == 0) {
        // fprintf(stderr, "Last semaphore trigger at %lu\n", (size_t)_this);
        // fflush(stderr);
        _last_time_func(last_args, entry.data);
        erase = true;
      }
    };

    map.lazy_emplace_l(key, /*if there is already an entry*/
                       std::move(onSecondOnwards),
                       std::move(onFirstToArrive));

    if (erase) {
      map.erase(key);
    }
  }
};

// ===========================================================================
// Ring variant (opt-in: -DIARA_RING_SEMAPHORE)
// ===========================================================================
//
// Same surface as KeyedSemaphore: identical `arrive(...)` signature and the
// same first/every/last templated callbacks. The map is replaced by a
// direct-mapped ring of `W` slots indexed by `key & (W-1)`, so a firing's
// arrivals coordinate through one lock-free, cache-local slot — no hashing,
// no submap lock, no insert/erase.
//
// Counting is accumulate-UP (`fetch_add`): the arrival that pushes the
// accumulated resources across `total_resources` is the completer. The first
// arrival to a free slot CASes `owner_key` from -1 and runs `first_time_func`;
// it publishes `ready` so later arrivals to the same key may run
// `every_time_func`/`last_time_func`.
//
// Slot lifetime extends to KERNEL completion, not gather completion: `arrive`
// never frees the slot. The caller invokes `release(key)` from `fire()` after
// the kernel has run, so the slot's inline `Data` (e.g. the kernel arg array)
// stays valid for the async kernel task. `W` therefore bounds the number of
// firings live *including execution* — the same antichain the allocator uses.
//
// Correctness is independent of `W`: if `key & (W-1)` is still owned by a
// different live key (W underestimate, or num_args wrap), the firing falls
// back to an overflow hash map with the same accumulate/complete semantics.
// `cleanup_func` frees any heap a slot's Data acquired (e.g. num_args > K).

inline void keyed_semaphore_pause() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  asm volatile("yield" ::: "memory");
#endif
}

template <template <class Key, class Value> class HashMap,
          class Data,
          class FirstArgs,
          class EveryTimeArgs,
          class LastArgs,
          void first_time_func(FirstArgs &, Data &),
          void every_time_func(EveryTimeArgs &, Data &),
          void last_time_func(LastArgs &, Data &),
          void cleanup_func(Data &),
          // When true, the slot frees itself at the completer (all resources
          // counted) instead of waiting for an explicit release(key). Use for
          // counters whose Data the firing does NOT read after completion (e.g.
          // the alloc semaphore, which fires on the FIRST arrival via
          // first_time_func and only counts to know when the entry is done).
          // Leaving the slot owned past completion would let the next same-key
          // dependent re-claim it and re-run first_time_func -> double-fire.
          bool AutoRelease = false>
struct KeyedSemaphoreRing {

  static constexpr i64 kDefaultCapacity = 1024;

  struct Slot {
    std::atomic<i64> owner_key{-1};
    std::atomic<i64> arrived{0};
    std::atomic<uint8_t> ready{0};
    Data data{};
  };

  // Overflow path: a still-live key collided on its ring slot. Behaves like the
  // map variant but defers erase to release(key) so inline Data outlives fire().
  struct OverflowEntry {
    i64 remaining_resources;
    Data data;
  };

  std::unique_ptr<Slot[]> ring;
  i64 mask = 0; // W - 1, W a power of two
  HashMap<i64, OverflowEntry> overflow{};

  static i64 next_pow2(i64 n) {
    i64 w = 1;
    while (w < n)
      w <<= 1;
    return w;
  }

  // Size the ring. `hint` = an upper bound on concurrently-live firings of this
  // node (e.g. total_iter_firings). Safe to call once before any arrive(); if
  // skipped the ring lazy-sizes to kDefaultCapacity on first arrive().
  void reserve(i64 hint) {
    if (ring)
      return;
    i64 w = next_pow2(std::min<i64>(std::max<i64>(hint, 1), kDefaultCapacity));
    ring = std::make_unique<Slot[]>(w);
    mask = w - 1;
  }

  void arrive(i64 key,
              i64 this_resources,
              i64 total_resources,
              FirstArgs &first_args,
              EveryTimeArgs &every_time_args,
              LastArgs &last_args) {

    assert(total_resources >= this_resources &&
           "Asking for more resources than total");

    if (!ring)
      reserve(kDefaultCapacity);

    Slot &slot = ring[key & mask];

    // Decide ring vs overflow. Routing for a key MUST be sticky: once a key's
    // first arrival went to the overflow map (its slot was busy with another
    // live key), every later arrival must too — otherwise, if the colliding key
    // frees the slot mid-gather, a later arrival would claim the now-free slot
    // and split this firing's accounting across ring+overflow (double/lost
    // fire). So when we don't already own the slot, consult the overflow map
    // before attempting a fresh claim. The `empty()` short-circuit keeps the
    // common no-wrap case (overflow always empty) lock-free.
    bool ring_path;
    i64 owner = slot.owner_key.load(std::memory_order_acquire);
    if (owner == key) {
      // Same firing, in the ring; first_time_func may still be running.
      while (slot.ready.load(std::memory_order_acquire) == 0)
        keyed_semaphore_pause();
      ring_path = true;
    } else if (owner == -1 && (overflow.empty() || !overflow.contains(key))) {
      i64 expected = -1;
      if (slot.owner_key.compare_exchange_strong(
              expected, key, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        // Claimed a free slot: build Data, then publish it.
        first_time_func(first_args, slot.data);
        slot.ready.store(1, std::memory_order_release);
        ring_path = true;
      } else if (expected == key) {
        // Another thread claimed for the same key: wait for publish.
        while (slot.ready.load(std::memory_order_acquire) == 0)
          keyed_semaphore_pause();
        ring_path = true;
      } else {
        ring_path = false; // lost the claim race to a different key
      }
    } else {
      ring_path = false; // slot busy with another key, or we are already in overflow
    }

    if (!ring_path) {
      arriveOverflow(key, this_resources, total_resources, first_args,
                     every_time_args, last_args);
      return;
    }

    // Write our contribution to Data FIRST, then publish it via fetch_add. The
    // acq_rel RMW release-sequence makes every producer's every_time_func write
    // visible to whichever arrival becomes the completer (its acquire), so
    // last_time_func sees a fully-populated arg array without a lock. (Doing
    // every_time_func after fetch_add would let the completer read stale args.)
    every_time_func(every_time_args, slot.data);
    i64 prev = slot.arrived.fetch_add(this_resources, std::memory_order_acq_rel);
    assert(prev + this_resources <= total_resources &&
           "Asking for more resources than available");
    if (prev + this_resources == total_resources) {
      last_time_func(last_args, slot.data); // sets may_fire + args span
      if constexpr (AutoRelease)
        freeSlot(slot);
    }
  }

  // Free the slot after the kernel has consumed its Data. Called from fire().
  // (Not used when AutoRelease is set — the slot frees itself at the completer.)
  void release(i64 key) {
    Slot &slot = ring[key & mask];
    if (slot.owner_key.load(std::memory_order_acquire) == key) {
      freeSlot(slot);
      return;
    }
    releaseOverflow(key);
  }

private:
  void freeSlot(Slot &slot) {
    cleanup_func(slot.data);
    slot.data = Data{};
    slot.arrived.store(0, std::memory_order_relaxed);
    slot.ready.store(0, std::memory_order_relaxed);
    slot.owner_key.store(-1, std::memory_order_release);
  }

  // Same accumulate-down semantics as the map variant, but erase is deferred to
  // releaseOverflow() so inline Data outlives the async kernel task.
  void arriveOverflow(i64 key,
                      i64 this_resources,
                      i64 total_resources,
                      FirstArgs &first_args,
                      EveryTimeArgs &every_time_args,
                      LastArgs &last_args) {
    bool completed = false;
    overflow.lazy_emplace_l(
        key,
        [&](typename decltype(overflow)::value_type &iter) {
          auto &e = iter.second;
          e.remaining_resources -= this_resources;
          assert(e.remaining_resources >= 0 && "Over-arrival on overflow entry");
          every_time_func(every_time_args, e.data);
          if (e.remaining_resources == 0) {
            last_time_func(last_args, e.data);
            completed = true;
          }
        },
        [&](typename decltype(overflow)::constructor &&ctor) {
          OverflowEntry e{.remaining_resources =
                              total_resources - this_resources,
                          .data = {}};
          first_time_func(first_args, e.data);
          every_time_func(every_time_args, e.data);
          if (e.remaining_resources == 0) {
            last_time_func(last_args, e.data);
            completed = true;
          }
          ctor(key, std::move(e));
        });
    if constexpr (AutoRelease)
      if (completed)
        releaseOverflow(key);
  }

  void releaseOverflow(i64 key) {
    // Run cleanup on the live entry (no-op constructor: never insert at
    // release), then erase. Rare path -> double lock is acceptable.
    overflow.lazy_emplace_l(
        key,
        [&](typename decltype(overflow)::value_type &iter) {
          cleanup_func(iter.second.data);
        },
        [&](typename decltype(overflow)::constructor &&) {});
    overflow.erase(key);
  }
};

} // namespace keyed_semaphore
#endif // IARA_RUNTIME_FIRST_COMPLETER_TASK_H
