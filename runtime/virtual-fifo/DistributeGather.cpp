#include "IaraRuntime/virtual-fifo/DistributeGather.h"
#include <cassert>


void iara_distribute(i64 seq, std::span<VirtualFIFO_Chunk> args){
  assert(args.size() > 1);

  auto input = args[0];

  i64 acc_size = input.data_size;

  for (size_t i = 1; i < args.size(); i++){
    args[i] = input.take_front(args[i].data_size);
    acc_size -= args[i].data_size;
  }

  assert(acc_size == 0);

}