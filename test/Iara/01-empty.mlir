// Empty graph

// RUN: iara-opt --iara-schedule %s | FileCheck %s

// CHECK-LABEL: func.func @__iara_run__()
iara.actor @main  {
  // CHECK-NEXT: return
  // CHECK-NEXT: }
} { flat }
// CHECK-LABEL: func.func @__iara_init__()
// CHECK-NEXT: return
// CHECK-NEXT: }