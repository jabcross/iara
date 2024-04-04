// RUN:   iara-opt --iara-schedule %s \
// RUN: | mlir-opt --convert-vector-to-scf \
// RUN:            --convert-linalg-to-loops \
// RUN:            --lower-affine \
// RUN:            --convert-scf-to-cf \
// RUN:            --canonicalize \
// RUN:            --cse \
// RUN:            --convert-vector-to-llvm \
// RUN:            --convert-math-to-llvm \
// RUN:            --expand-strided-metadata \
// RUN:            --lower-affine \
// RUN:            --finalize-memref-to-llvm \
// RUN:            --convert-func-to-llvm="use-bare-ptr-memref-call-conv=1" \
// RUN:            --convert-index-to-llvm \
// RUN:            --convert-openmp-to-llvm \
// RUN:            --reconcile-unrealized-casts \
// RUN: | mlir-translate --mlir-to-llvmir \
// RUN:                  --mlir-print-debuginfo \
// RUN:                  > 04-multithread.ll \
// RUN: && clang++ -lomp 04-multithread.ll "`dirname %s`/04-multithread.cpp" && ./a.out \
// RUN: | FileCheck %s

// Two nodes

iara.actor @a {
  iara.out : tensor<1xi64>
  iara.dep
} { kernel }
iara.actor @b {
  iara.out : tensor<1xi64>
  iara.dep
} { kernel }
iara.actor @c {
  %1 = iara.in : tensor<1xi64>
  %2 = iara.in : tensor<1xi64>
  iara.dep
} { kernel }

iara.actor @main  {
  %1 = iara.node @a out : tensor<1xi64>
  %2 = iara.node @b out : tensor<1xi64>
  iara.node @c in %1, %2 : tensor<1xi64> , tensor<1xi64>
  iara.dep
} { flat }

// CHECK: Ran in different threads.