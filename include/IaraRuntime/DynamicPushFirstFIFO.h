#ifndef IARA_RUNTIME_DYNAMIC_PUSH_FIRST_FIFO_H
#define IARA_RUNTIME_DYNAMIC_PUSH_FIRST_FIFO_H

#include "Iara/Util/types.h"
#include <cstdint>
#include <cstdlib>
#include <memory>

// FIFO logic with underlying hash map. Producers do not need to arrive at any
// specific order. Consumers must wait for their turn. Pushing and popping may
// be of arbitrary size.
struct DynamicPushFirstFifo {

  struct Impl;
  Impl *pimpl;

  // Pushes a new block to the FIFO with the given sequence number.
  void push(i64 size, i64 sequence_number, byte *data);

  // Copies the front of the queue into the given write buffer. Blocks until
  // there is data.
  byte *pop(i64 size, i64 sequence_number);

  static DynamicPushFirstFifo *create();
};

extern "C" void *iara_fifo_runtime_DynamicPushFirstFifo_create();

extern "C" void iara_fifo_runtime_DynamicPushFirstFifo_push(void *fifo,
                                                            i64 size,
                                                            i64 sequence_number,
                                                            byte *data);

extern "C" byte *
iara_fifo_runtime_DynamicPushFirstFifo_pop(void *fifo, i64 size,
                                           i64 sequence_number);

#endif
