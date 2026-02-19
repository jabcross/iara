// RUN: export SCHEDULER_MODE=$SCHEDULER_MODE
// RUN: pwd >&2
// RUN: build-single-test.sh .
// RUN: pwd >&2
// RUN: ls build >&2
// RUN: ./build/a.out > ./build/output.txt
// RUN: FileCheck %s < ./build/output.txt

iara.actor @nested {
  %0 = iara.in : i32
  %1 = iara.node @b in %0: i32 out i32
  iara.out (%1: i32)
}
iara.actor @run  {
  %0 = iara.node @a out i32
  %1 = iara.node @nested in %0: i32 out i32
  iara.node @c in %1 : i32
}

// CHECK: 30