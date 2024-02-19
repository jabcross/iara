//===- IaraPasses.h - Iara passes  ------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef IARA_IARAPASSES_H
#define IARA_IARAPASSES_H

#include "Iara/IaraDialect.h"
#include "Iara/IaraOps.h"
#include "mlir/Pass/Pass.h"
#include <memory>

namespace mlir {
namespace iara {
#define GEN_PASS_DECL
#include "Iara/IaraPasses.h.inc"

#define GEN_PASS_REGISTRATION
#include "Iara/IaraPasses.h.inc"
} // namespace iara
} // namespace mlir

#endif
