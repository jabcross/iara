// RUN: python "`dirname %s`/generate_topology.py" \
// RUN: | iara-opt --iara-schedule  \
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
// RUN: | clang++ -lomp -xir - "`dirname %s`/05-cholesky.cpp" && ./a.out \
// RUN: | FileCheck %s