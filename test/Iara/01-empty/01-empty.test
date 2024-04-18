// Empty graph

// RUN: iara-opt --iara-schedule %s | FileCheck %s

// CHECK-LABEL: func.func @main()
iara.actor @main  {
  iara.dep
  // CHECK-NEXT: return
  // CHECK-NEXT: }
} { flat }