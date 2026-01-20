//===- llvm/Support/ToolInterface.h - Common tool interface -----*- C++ -*-===//
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

#ifndef LLVM_SUPPORT_TOOLINTERFACE_H
#define LLVM_SUPPORT_TOOLINTERFACE_H

#include "llvm/Support/MemoryBufferRef.h"
#include <optional>

namespace llvm {
class LLVMTool {
public:
  virtual ~LLVMTool() {}

  virtual int run(int Argc, char **Argv,
                  std::optional<MemoryBufferRef> StdinOverride) = 0;

  virtual void resetState() = 0;
};
} // namespace llvm

#endif // LLVM_SUPPORT_TOOLINTERFACE_H
