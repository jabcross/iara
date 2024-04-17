// RUN: python "`dirname %s`/generate_topology.py" 4 4 \
// RUN: | iara-opt --iara-schedule  \
// RUN: | mlir-to-llvmir.sh \
// RUN: | clang++ -lomp -lblas -xir - -xc "`dirname %s`/main.c" && ./a.out \
// RUN: | FileCheck %s