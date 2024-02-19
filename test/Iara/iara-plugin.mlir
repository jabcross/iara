// RUN: mlir-opt %s --load-dialect-plugin=%iara_libs/IaraPlugin%shlibext --pass-pipeline="builtin.module(iara-switch-bar-foo)" | FileCheck %s

module {
  // CHECK-LABEL: func @foo()
  func.func @bar() {
    return
  }

  // CHECK-LABEL: func @iara_types(%arg0: !iara.custom<"10">)
  func.func @iara_types(%arg0: !iara.custom<"10">) {
    return
  }
}
