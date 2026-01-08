// TODO(BStott) file header

#ifndef LLVM_SUPPORT_DAEMONDRIVER_H
#define LLVM_SUPPORT_DAEMONDRIVER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <functional>

namespace llvm {
/// "Main" function for a tool invoked by the daemon. This should not call
/// `InitLLVM`; this should be run before `daemonMain`. The invoke function is
/// responsible for resetting any managed global state, for example statistics
/// and debug counters, to prevent any state from leaking to the next
/// invocation.
using ToolInvokeFn =
    std::function<int(ArrayRef<const char *> Args, MemoryBufferRef Input)>;

/// TODO(BStott) document.
LLVM_ABI int daemonMain(ToolInvokeFn InvokeTool, ArrayRef<const char *> Args);
} // namespace llvm

#endif // LLVM_SUPPORT_DAEMONDRIVER_H
