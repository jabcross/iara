#include "IaraRuntime/FirstLastSemaphore.h"
#include "IaraRuntime/Chunk.h"
#include <atomic>
#include <cstdlib>

namespace first_last_semaphore {

template <class Data, class FirstArgs, class EveryTimeArgs, class LastArgs>
void first_last_semaphore::
    FirstLastSemaphoreMap<Data, FirstArgs, EveryTimeArgs, LastArgs>::arrive(
        i64 id, i64 this_resources, i64 total_resources, FirstArgs &first_args,
        EveryTimeArgs &every_time_args, LastArgs &last_args) {
  assert(total_resources >= this_resources);

  // todo: avoid scheduling if trivial

  struct detail {
    static inline bool common(FirstLastSemaphoreMap &map, Entry &entry,
                              EveryTimeArgs &et_args, LastArgs &l_args) {
      map.every_time_func(et_args, entry.data);
      if (entry.remaining_resources == 0) {
        map.last_time_func(l_args, entry.data);
        return true;
      }
      return false;
    }
  };

  bool erase = false;

  map.lazy_emplace_l(
      id, /*if there is already an entry*/
      [&every_time_args, &last_args, _this = this,
       this_resources = this_resources, &erase](Entry &entry) {
        entry.remaining_resources -= this_resources;
        erase = detail::common(*_this, entry, last_args);
      }, /*if this is the first dependency */
      [&first_args, &every_time_args, &last_args, _this = this,
       this_resources = this_resources, &erase](Entry &entry) {
        _this->first_time_func(first_args, entry.data);
        erase = detail::common(*_this, entry, last_args);
      });

  if (erase) {
    map.erase(id);
  }
}
} // namespace first_last_semaphore
