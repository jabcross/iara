#ifndef IARA_RUNTIME_FIRST_COMPLETER_TASK_H
#define IARA_RUNTIME_FIRST_COMPLETER_TASK_H

#include "Iara/Util/Types.h"

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
  std::function<void(FirstArgs &, Data &)>
      first_time_func; // called by the first dependency
  std::function<void(EveryTimeArgs &, Data &)>
      every_time_func; // called by every dependency
  std::function<void(LastArgs &, Data &)>
      last_time_func; // called by the last depenency

  // The first dependency to access this id will execute (`first`). The last one
  // will execute `last`. Each dependency will decrement the counter.
  void arrive(i64 id, i64 this_resources, i64 total_resources,
              FirstArgs &first_args, EveryTimeArgs &every_time_args,
              LastArgs &last_args);
};

} // namespace first_last_semaphore

#endif // IARA_RUNTIME_FIRST_COMPLETER_TASK_H
