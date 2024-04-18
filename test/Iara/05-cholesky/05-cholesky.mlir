// RUN: python "`dirname %s`/generate_topology.py" 8 8 \
// RUN: | iara-opt --iara-schedule  \
// RUN: | mlir-to-llvmir.sh \
// RUN: | clang++ -DDIM=8 -DNB=8 -lomp -lblas -xir - -xc "`dirname %s`/main.c" && ./a.out \
// RUN: | FileCheck %s
// CHECK: Factorization correct