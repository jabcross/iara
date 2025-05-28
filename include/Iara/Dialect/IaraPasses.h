//===- IaraPasses.h - Iara passes  ------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef IARA_IARAPASSES_H
#define IARA_IARAPASSES_H

#include "Iara/Dialect/IaraDialect.h"
#include "Iara/Dialect/IaraOps.h"
#include "Iara/Passes/LowerToTasksPass.h"
#include "mlir/Pass/Pass.h"
#include <memory>

namespace iara {
namespace passes {

#define GEN_PASS_DECL
#include "Iara/Dialect/IaraPasses.h.inc"

#define GEN_PASS_REGISTRATION
#include "Iara/Dialect/IaraPasses.h.inc"

} // namespace passes
} // namespace iara

#endif
