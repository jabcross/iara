// Regression test for broadcast of struct types (element size > 8 bytes).
// With the bug: num_copies = getTypeSize(!llvm.ptr) / getTypeSize(tensor<1x!TestStruct>)
//             = 8 / 24 = 0 → consumer_b gets uninitialized memory → test fails.
// With the fix: num_copies = getTypeSize(tensor<1x!TestStruct>) / getTypeSize(...)
//             = 24 / 24 = 1 → both consumers receive correct copy → test passes.

!TestStruct = !llvm.struct<"struct.TestStruct", (i32, i32, f64, i32)>

iara.actor @run  {
  %data = iara.node @producer out tensor<1x!TestStruct>
  iara.node @consumer_a in %data : tensor<1x!TestStruct>
  iara.node @consumer_b in %data : tensor<1x!TestStruct>
} { flat }
// CHECK-DAG: consumer_a ok
// CHECK-DAG: consumer_b ok
