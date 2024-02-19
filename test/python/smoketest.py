# RUN: %python %s | FileCheck %s

from mlir_iara.ir import *
from mlir_iara.dialects import builtin as builtin_d, iara as iara_d

with Context():
    iara_d.register_dialect()
    module = Module.parse(
        """
    %0 = arith.constant 2 : i32
    %1 = iara.foo %0 : i32
    """
    )
    # CHECK: %[[C:.*]] = arith.constant 2 : i32
    # CHECK: iara.foo %[[C]] : i32
    print(str(module))
