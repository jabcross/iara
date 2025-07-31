// RUN: export SCHEDULER_MODE=$SCHEDULER_MODE
// RUN: pwd >&2
// RUN: build-single-test.sh .
// RUN: pwd >&2
// RUN: ls build >&2
// RUN: ./build/a.out > ./build/output.txt
// RUN: FileCheck %s < ./build/output.txt

// Two nodes

iara.actor @run  {
  %1 = iara.node @a out i32
  iara.node @b in %1 : i32
  iara.node @b in %1 : i32
  %2 = iara.node @c inout %1 : i32 out i32
  %3 = iara.node @c inout %1 : i32 out i32
  iara.node @b in %2 : i32
  iara.node @b in %3 : i32
} { flat }

// CHECK: There were 0 mistakes when copying the value.
// CHECK: The value was copied 3 times and reused 1 time.