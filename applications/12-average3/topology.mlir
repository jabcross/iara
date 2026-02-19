// RUN: export SCHEDULER_MODE=$SCHEDULER_MODE
// RUN: pwd >&2
// RUN: build-single-test.sh .
// RUN: pwd >&2
// RUN: ls build >&2
// RUN: ./build/a.out > ./build/output.txt
// RUN: FileCheck %s < ./build/output.txt

// backward delay

iara.actor @run  {
  %n0 = iara.node @a out tensor<9xf32>
  %e1 = iara.edge %n0 : tensor<9xf32> -> f32
  %n1_0,%n1_1 = iara.node @b in %e1: f32, %e2 : tensor<2xf32> out f32, tensor<2xf32>
  %e2 = iara.edge %n1_1 : tensor<2xf32> -> tensor<2xf32> {delay = array<f32: 0.0, 0.0>}
  %e3 = iara.edge %n1_0 : f32 -> tensor<9xf32>
  iara.node @c in %e3: tensor<9xf32>
} { }

// CHECK: 0 1 3 3 3 3 3 3 3
// CHECK: 3 3 3 3 3 3 3 3 3