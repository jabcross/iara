#ifndef IARA_RUNTIME_FIRST_COMPLETER_TASK_H
#define IARA_RUNTIME_FIRST_COMPLETER_TASK_H

#include "Iara/Util/Types.h"
#include <cstdio>
#include <iostream>

namespace first_last_semaphore {

// A semaphore to manage dependencies, delegating the creation of the task to
// the first dependency to arrive.
template <class Data, class FirstArgs, class EveryTimeArgs, class LastArgs>
struct FirstLastSemaphoreMap {
  struct Entry {
    i64 remaining_resources;
    Data data;
  };

  ParallelHashMap<i64, Entry> map{};
  using Iterator = ParallelHashMap<i64, Entry>::iterator;
  ::std::function<void(FirstArgs &, Data &)>
      first_time_func; // called by the first dependency
  std::function<void(EveryTimeArgs &, Data &)>
      every_time_func; // called by every dependency
  std::function<void(LastArgs &, Data &)>
      last_time_func; // called by the last depenency

  // The first dependency to access this id will execute (`first`). The last one
  // will execute `last`. Each dependency will decrement the counter.
  void arrive(i64 id, i64 this_resources, i64 total_resources,
              FirstArgs &first_args, EveryTimeArgs &every_time_args,
              LastArgs &last_args) {

    assert(total_resources >= this_resources &&
           "Asking for more resources than total");

    if (this_resources == total_resources) {

      // debug
      fprintf(stderr,
              "Trivial semaphore at %lu\n"
              "               id = %ld\n"
              "   this_resources = %ld\n"
              "  total_resources = %ld\n",
              (size_t)this, id, this_resources, total_resources);
      fflush(stderr);

      // skip the semaphore
      Data data{};
      first_time_func(first_args, data);
      every_time_func(every_time_args, data);
      last_time_func(last_args, data);
      return;
    }

    assert(total_resources > this_resources);

    bool erase = false;

    auto onFirstToArrive = [id, &first_args, &every_time_args, _this = this,
                            this_resources, total_resources,
                            &erase](decltype(map)::constructor &&ctor) {
      fprintf(stderr,
              "First semaphore trigger at %lu\n"
              "               id = %ld\n"
              "   this_resources = %ld\n"
              "  total_resources = %ld\n",
              (size_t)_this, id, this_resources, total_resources);
      fflush(stderr);

      Entry new_value{.remaining_resources = total_resources - this_resources,
                      .data = {}};
      _this->first_time_func(first_args, new_value.data);
      _this->every_time_func(every_time_args, new_value.data);
      ctor(id, std::move(new_value));
    };

    auto onSecondOnwards = [id, total_resources, &every_time_args, &last_args,
                            _this = this, this_resources,
                            &erase](decltype(map)::value_type &iter) {
      fprintf(stderr,
              "Subsequent semaphore trigger at %lu\n"
              "               id = %ld\n"
              "   this_resources = %ld\n"
              "  total_resources = %ld\n",
              (size_t)_this, id, this_resources, total_resources);
      fflush(stderr);
      auto &entry = iter.second;
      entry.remaining_resources -= this_resources;
      assert(entry.remaining_resources >= 0 &&
             "Asking for more resources than available");
      _this->every_time_func(every_time_args, entry.data);
      if (entry.remaining_resources == 0) {
        fprintf(stderr, "Last semaphore trigger at %lu\n", (size_t)_this);
        fflush(stderr);
        _this->last_time_func(last_args, entry.data);
      }
    };

    map.lazy_emplace_l(id, /*if there is already an entry*/
                       std::move(onSecondOnwards), std::move(onFirstToArrive));

    if (erase) {
      map.erase(id);
    }
  }
};

} // namespace first_last_semaphore
#endif // IARA_RUNTIME_FIRST_COMPLETER_TASK_H
