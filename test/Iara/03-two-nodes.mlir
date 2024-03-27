// RUN: iara-opt --iara-schedule %s -split-input-file | FileCheck %s

// Two nodes

iara.actor @a {
  iara.out : tensor<1xi32>
} { kernel }
iara.actor @b {
  %1 = iara.in : tensor<1xi32>
} { kernel }
// CHECK-LABEL: func.func @__iara_run__()
// CHECK-NEXT: call @a() : () -> ()
// CHECK-NEXT: call @b() : () -> ()
// CHECK-NEXT: return
// CHECK-NEXT: }
// CHECK-LABEL: func.func @__iara_init__()
// CHECK-NEXT: return
// CHECK-NEXT: }
iara.actor @main  {
  %1 = iara.node @a out : tensor<1xi32>
  iara.node @b in %1 : tensor<1xi32>
} { flat }