#ifndef IARA_RUNTIME_FIFO_SCHEDULER_H
#define IARA_RUNTIME_FIFO_SCHEDULER_H

#include "Util/types.h"
#include "external/concurrentqueue/blockingconcurrentqueue.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>

using std::byte;

namespace iara_fifo_runtime {

template <i64 N, class... Args> struct SpreadArgsFuncType {
  using func_type = SpreadArgsFuncType<N - 1, byte *, Args...>::func_type;
};

template <class... Args> struct SpreadArgsFuncType<0, Args...> {
  static void (*func)(Args...);
  using func_type = decltype(func);
};

template <i64 N, i64... Args> struct SpreadArgs {
  using type = SpreadArgs<N - 1, N, Args...>::type;
};

template <i64... Args> struct SpreadArgs<0, Args...> {
  using type = struct impl {
    static void call(void *func, byte *args) {
      auto casted_func = (SpreadArgsFuncType<sizeof...(Args)>::func_type);
      casted_func(args[Args]...);
    };
  };
};

struct FIFO {
  std::atomic_int next_prod_iter;
  std::atomic_int next_cons_iter;

  i64 prod_rate;
  i64 cons_rate;

  moodycamel::BlockingConcurrentQueue<byte> *queue;

  void pop(i64 iter,
           byte *); // Blocks until available; copies into given pointer.
  void push(i64 iter, byte *); // Copies from given pointer.
};

extern "C" FIFO *iara_make_FIFO(i64 prod_rate, i64 cons_rate, i64 delay_size,
                                byte *delay_data);

template <i64 num_args> struct Node {
  FIFO *incoming_fifos;
  FIFO *outgoing_fifos;
  i64 num_incoming_fifos;
  i64 num_outgoing_fifos;
  i64 total_firings;
  SpreadArgsFuncType<num_args> kernel;

  void fire();
};

template <i64 num_args>
Node<num_args> *iara_make_Node(FIFO *incoming_fifos, FIFO *outgoing_fifos,
                               i64 num_incoming_fifos, i64 num_outgoing_fifos,
                               i64 total_firings, void *kernel);

} // namespace iara_fifo_runtime

#endif // IARA_RUNTIME_FIFO_SCHEDULER_H