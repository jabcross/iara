// RUN: mlir-opt %s --load-pass-plugin=%iara_libs/IaraPlugin%shlibext --pass-pipeline="builtin.module(iara-switch-bar-foo)" | FileCheck %s

module {
  // CHECK-LABEL: func @foo()
  func.func @bar() {
    return
  }

  // CHECK-LABEL: func @abar()
  func.func @abar() {
    return
  }
}
