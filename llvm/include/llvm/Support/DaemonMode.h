// TODO(BStott) file header

#ifndef LLVM_SUPPORT_DAEMONMODE_H
#define LLVM_SUPPORT_DAEMONMODE_H

#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <functional>

namespace llvm {
/// TODO(BStott) document.
using ToolInvokeFn =
    std::function<int(int Argc, char **Argv, MemoryBufferRef Input)>;

/// TODO(BStott) document.
LLVM_ABI void runDaemonMode(ToolInvokeFn InvokeTool);
} // namespace llvm

#endif // LLVM_SUPPORT_DAEMONMODE_H
