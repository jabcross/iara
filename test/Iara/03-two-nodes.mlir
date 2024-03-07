// RUN: iara-opt --iara-schedule %s -split-input-file | FileCheck %s

// Two nodes

iara.kernel @a {
  iara.out tensor<1xi32>
}
iara.kernel @b {
  iara.in tensor<1xi32>
}
// CHECK-LABEL: func.func @__iara_run__()
// CHECK: call @a() : () -> ()
// CHECK: call @b() : () -> ()
// CHECK: return
// CHECK: }
iara.graph @main  {
  %1 = iara.node @a out tensor<1xi32>
  iara.node @b in %1:tensor<1xi32> out none
} { flat = true }
// CHECK-LABEL: func.func @__iara_init__()
// CHECK-NEXT: return
// CHECK-NEXT: }