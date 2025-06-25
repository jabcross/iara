#ifndef IARA_RUNTIME_FIRST_COMPLETER_TASK_H
#define IARA_RUNTIME_FIRST_COMPLETER_TASK_H

#include "Iara/Util/CommonTypes.h"
#include <cstdio>
#include <iostream>

namespace keyed_semaphore {

// A semaphore to manage dependencies, delegating the creation of the task to
// the first dependency to arrive.
template <class Data,
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

  ParallelHashMap<i64, Entry> map{};
  using Iterator = ParallelHashMap<i64, Entry>::iterator;

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

} // namespace keyed_semaphore
#endif // IARA_RUNTIME_FIRST_COMPLETER_TASK_H
