//===- ExampleDaemon.cpp - Example tool for testing daemon driver
//----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is an example tool used for testing the daemon driver functionality.
// The tool reads input from stdin character-by-character and prints
// uppercase letters on `stderr` and everything else on `stdout`. It
// returns the number of times a letter was printed to `stderr`. It also takes
// one command line argument which causes it to print lowercase letters to
// `stderr` instead.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DaemonDriver.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolInterface.h"
#include <cctype>
using namespace llvm;

// Global variable used to check that `resetState` is properly called between
// invocations.
constexpr int PersistentStateInit = 0;
static int PersistentState = PersistentStateInit;

static cl::opt<bool> SeparateLowercaseInstead("separate-lowercase-instead",
                                              cl::init(false));

class ExampleTool : public LLVMTool {
public:
  virtual int run(int Argc, char **Argv,
                  std::optional<MemoryBufferRef> StdinOverride) override {
    // Make sure that `PersistentState` has been reset.
    assert(PersistentState == PersistentStateInit &&
           "Persistent state should have been reset.");
    PersistentState = 1;

    cl::ParseCommandLineOptions(Argc, Argv);

    // Read standard input or get it from `StdinOverride`.
    std::unique_ptr<MemoryBuffer> StdinBuf;
    MemoryBufferRef StdinContent;
    if (StdinOverride) {
      StdinContent = *StdinOverride;
    } else {
      ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
          MemoryBuffer::getFileOrSTDIN("-", /*IsText=*/false);
      assert(!FileOrErr.getError());
      StdinBuf.swap(FileOrErr.get());
      StdinContent = StdinBuf->getMemBufferRef();
    }

    int StderrCount = 0;
    for (const char Char : StdinContent.getBuffer()) {
      if (SeparateLowercaseInstead ? islower(Char) : isupper(Char)) {
        llvm::errs() << Char;
        llvm::errs().flush();
        StderrCount += 1;
      } else {
        llvm::outs() << Char;
        llvm::outs().flush();
      }
    }

    return StderrCount;
  };

  virtual void resetState() override {
    PersistentState = PersistentStateInit;
    cl::ResetCommandLineParser();
  }
};

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  ExampleTool Tool;
  return runWithDaemonSupport(Tool, argc, argv);
}
