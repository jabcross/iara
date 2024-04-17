// RUN:   iara-opt --iara-schedule %s \
// RUN: | mlir-to-llvmir.sh \
// RUN: | clang -lomp -lpthread -xir - -xc "`dirname %s`/03-two-nodes.c" && ./a.out \
// RUN: | FileCheck %s

// Two nodes

iara.actor @a {
  iara.out : tensor<1xi32>
  iara.dep
} { kernel }
iara.actor @b {
  %1 = iara.in : tensor<1xi32>
  iara.dep
} { kernel }
iara.actor @run  {
  %1 = iara.node @a out : tensor<1xi32>
  iara.node @b in %1 : tensor<1xi32>
  iara.dep
} { flat }

// CHECK: Hello World!