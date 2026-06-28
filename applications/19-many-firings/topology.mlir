// High firing count: a producer that fires N=2048 times feeding one large
// gather firing. Exercises the ring KeyedSemaphore beyond its W cap (default
// 1024): the producer node wraps its slot ring and spills to the overflow map,
// while the single consumer firing accumulates 2048 arrivals into one slot
// (accumulate-up contention).

iara.actor @run  {
  %0 = iara.node @prod out tensor<1xi32>
  %1 = iara.edge %0 : tensor<1xi32> -> tensor<2048xi32>
  iara.node @cons in %1 : tensor<2048xi32>
} { }
