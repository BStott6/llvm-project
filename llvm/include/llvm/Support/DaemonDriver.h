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
    std::function<int(int Argc, char **Argv, MemoryBufferRef Input)>;

/// Sets up the daemon-related command line options. This must be called before
/// parsing command line options, and also before `daemonModeEnabled` or
/// `runDaemonMode`.
LLVM_ABI void initializeDaemonOptions();

/// Returns true if the `--daemon` flag was passed.
LLVM_ABI bool daemonModeEnabled();

/// TODO(BStott) document.
LLVM_ABI int runDaemonMode(ToolInvokeFn InvokeTool);
} // namespace llvm

#endif // LLVM_SUPPORT_DAEMONDRIVER_H
