// RUN: iara-opt --iara-schedule %s -split-input-file | FileCheck %s

// Single node

iara.kernel @a {
}
// CHECK-LABEL: func.func @__iara_run__()
// CHECK: call @a() : () -> ()
// CHECK: return
// CHECK: }
iara.graph @main  {
  iara.node @a out none
} { flat = true }
// CHECK-LABEL: func.func @__iara_init__()
// CHECK-NEXT: return
// CHECK-NEXT: }