//===- DaemonMode.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_DAEMONMODE_H
#define LLVM_SUPPORT_DAEMONMODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Compiler.h"
#include <functional>
#include <string>

namespace llvm {
struct DaemonInvocationContext {
  // String used as input instead of stdin 
  std::string InputContent;
};

// Runs `F` in Daemon mode: Commands are accepted from `cin`, which are parsed into
// arguments and then F is called with the given arguments.
// Used for `llc --daemon` etc.
LLVM_ABI int runDaemonMode(std::function<int(int argc, char **argv, DaemonInvocationContext DIC)> F);
} // namespace llvm

#endif 
