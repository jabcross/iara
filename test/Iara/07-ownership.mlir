// RUN:   iara-opt --iara-schedule %s \
// RUN: | FileCheck %s

// Two nodes

iara.actor @a {
  iara.out : tensor<1xi32>
  iara.out : tensor<1xi32>
  iara.out : tensor<1xi32>
  iara.out : tensor<1xi32>
  iara.dep
} { kernel }
iara.actor @b {
  %1 = iara.in : tensor<1xi32>
  iara.dep
} { kernel }
iara.actor @c {
  %1 = iara.in inout : tensor<1xi32>
  iara.dep
} { kernel }
iara.actor @main  {
  %1, %2, %3, %4 = iara.node @a out : tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>
  %5 = iara.node @c inout %1 : tensor<1xi32> out: tensor<1xi32>
  %6 = iara.node @c inout %2 : tensor<1xi32> out: tensor<1xi32>
  iara.node @b inout %3 : tensor<1xi32>
  iara.node @b inout %4 : tensor<1xi32>
  iara.dep
} { flat }

// CHECK: __iara_copy__