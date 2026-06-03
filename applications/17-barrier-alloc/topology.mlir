// RUN: export SCHEDULER_MODE=$SCHEDULER_MODE
// RUN: build-single-test.sh .
// RUN: ./build/a.out > ./build/output.txt
// RUN: FileCheck %s < ./build/output.txt

// Regression: alloc double-fire on a memmove node with two inout chains.
//
// `barrier` is not in-place: it has a pure-in and a pure-out, so after
// alloc/dealloc promotion it carries two separate inout chains — an input
// chain (src -> copyA -> barrier -> dealloc) and an output chain
// (alloc -> barrier -> copyB -> dealloc). The barrier's *output* buffer is
// therefore consumed downstream by copyB. Before the fix, both `barrier` and
// `copyB` pinged the chain-head alloc via prime()->ensureAlloc(); with the
// trivial (single-dependent) semaphore that re-fired the alloc, double-
// allocating each block. The node then fired with a null arg -> SIGSEGV
// (this is the BarrierTranspose2x crash in 08-sift, minimised).
//
// Multi-rate edges (96<->16) make every middle node fire 6 times, matching the
// per-gaussian-layer firing of the real barrier.

iara.actor @run  {
  %n0 = iara.node @src out tensor<96xf32>
  %e1 = iara.edge %n0 : tensor<96xf32> -> tensor<16xf32>
  %n1 = iara.node @copyA in %e1: tensor<16xf32> out tensor<16xf32>
  %e2 = iara.edge %n1 : tensor<16xf32> -> tensor<16xf32>
  %n2 = iara.node @barrier in %e2: tensor<16xf32> out tensor<16xf32>
  %e3 = iara.edge %n2 : tensor<16xf32> -> tensor<16xf32>
  %n3 = iara.node @copyB in %e3: tensor<16xf32> out tensor<16xf32>
  %e4 = iara.edge %n3 : tensor<16xf32> -> tensor<96xf32>
  iara.node @sink in %e4: tensor<96xf32>
} { }

// CHECK: OK 96
