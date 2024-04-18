// RUN:   iara-opt --iara-schedule %s \
// RUN: | mlir-to-llvmir.sh \
// RUN: | clang++ -lomp -xir - -xc++ "`dirname %s`/04-multithread.cpp" && ./a.out \
// RUN: | FileCheck %s

// Two nodes

iara.actor @a {
  iara.out : tensor<1xi64>
} { kernel }
iara.actor @b {
  iara.out : tensor<1xi64>
} { kernel }
iara.actor @c {
  %1 = iara.in : tensor<1xi64>
  %2 = iara.in : tensor<1xi64>
} { kernel }

iara.actor @run  {
  %1 = iara.node @a out : tensor<1xi64>
  %2 = iara.node @b out : tensor<1xi64>
  iara.node @c in %1, %2 : tensor<1xi64> , tensor<1xi64>
  iara.dep
} { flat }

// CHECK: Ran in different threads.