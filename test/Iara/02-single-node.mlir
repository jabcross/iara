// RUN: iara-opt --iara-schedule %s -split-input-file | FileCheck %s

// Single node

iara.actor @a {
} { kernel }
// CHECK-LABEL: func.func @__iara_run__()
// CHECK: call @a() : () -> ()
// CHECK: return
// CHECK: }
iara.actor @main  {
  iara.node @a out none
} { flat }
// CHECK-LABEL: func.func @__iara_init__()
// CHECK-NEXT: return
// CHECK-NEXT: }