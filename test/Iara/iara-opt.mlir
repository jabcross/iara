// Empty graph

// RUN: iara-opt --iara-schedule %s | FileCheck %s

module {
  // CHECK-LABEL: func.func @__iara_run__()
  iara.graph @main  {
    // CHECK-NEXT: return
    // CHECK-NEXT: }
  } { flat = true }
  // CHECK-LABEL: func.func @__iara_init__()
  // CHECK-NEXT: return
  // CHECK-NEXT: }
}