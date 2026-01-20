//===- llvm/Support/DaemonDriver.h - Daemon driver interface ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TODO document me
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_DAEMONDRIVER_H
#define LLVM_SUPPORT_DAEMONDRIVER_H

#include "llvm/Support/Compiler.h"
#include "llvm/Support/ToolInterface.h"

namespace llvm {
LLVM_ABI int runWithDaemonSupport(LLVMTool &Tool, int Argc, char **Argv);
} // namespace llvm

#endif // LLVM_SUPPORT_DAEMONDRIVER_H
