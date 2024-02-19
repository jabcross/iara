// RUN: iara-opt %s | iara-opt | FileCheck %s

module {
    // CHECK-LABEL: func @bar()
    func.func @bar() {
        %0 = arith.constant 1 : i32
        // CHECK: %{{.*}} = iara.foo %{{.*}} : i32
        %res = iara.foo %0 : i32
        return
    }

    // CHECK-LABEL: func @iara_types(%arg0: !iara.custom<"10">)
    func.func @iara_types(%arg0: !iara.custom<"10">) {
        return
    }
}
