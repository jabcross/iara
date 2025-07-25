//===- iara-opt.cpp ---------------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraPasses.h"
#include "Iara/Passes/Canonicalize/IaraCanonicalizePass.h"
#include "Iara/Passes/RingBuffer/RingBufferSchedulerPass.h"
#include "Iara/Passes/VirtualFIFO/VirtualFIFOSchedulerPass.h"
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>

int main(int argc, char **argv) {

  mlir::DialectRegistry registry;
  registry.insert<iara::dialect::IaraDialect,
                  mlir::arith::ArithDialect,
                  mlir::func::FuncDialect,
                  mlir::LLVM::LLVMDialect>();

  iara::passes::canonicalize::registerIaraCanonicalizePass();
  iara::passes::registerFlattenPass();
  iara::passes::virtualfifo::registerVirtualFIFOSchedulerPass();
  iara::passes::ringbuffer::registerRingBufferSchedulerPass();

  // // Add the following to include *all* MLIR Core dialects, or selectively
  // include what you need like above. You only need to register dialects that
  // will be *parsed* by the tool, not the one generated
  // registerAllDialects(registry);

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "Iara optimizer driver\n", registry));
}
