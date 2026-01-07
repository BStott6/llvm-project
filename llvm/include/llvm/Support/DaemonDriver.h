// TODO(BStott) file header

#ifndef LLVM_SUPPORT_DAEMONDRIVER_H
#define LLVM_SUPPORT_DAEMONDRIVER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <functional>

namespace llvm {
/// TODO(BStott) document.
using ToolInvokeFn =
    std::function<int(int Argc, char **Argv, MemoryBufferRef Input)>;

/// TODO(BStott) document.
LLVM_ABI int daemonMain(ToolInvokeFn InvokeTool, ArrayRef<const char *> Args);
} // namespace llvm

#endif // LLVM_SUPPORT_DAEMONDRIVER_H
