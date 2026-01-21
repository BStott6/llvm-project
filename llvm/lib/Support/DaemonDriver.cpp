//===- llvm/Support/DaemonDriver.cpp - Daemon driver interface --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the interface for tools to be run in "daemon mode",
// following the IPC protocol as described in docs/DaemonMode.rst.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/DaemonDriver.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ScopedFileRedirect.h"
#include <cstdio>
#include <cstdlib>
#include <optional>

#if defined _WIN32
#include <io.h>
#endif

// Standard stream fileno macros are not defined on Windows.
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

using namespace llvm;

namespace {
/// Status code returned if the daemon fails to initialize, for example due to
/// incorrect command line arguments.
constexpr int StatusInitError = 2;
/// Status code returned if the daemon receives a malformed command.
constexpr int StatusCommandError = 3;

static bool DaemonMode;
static int DaemonStatusFd;
static int DaemonStatusHandle;

/// This should only be used before the status pipe is set up - after,
/// errors are reported to the user via the status pipe.
[[noreturn]] void reportInitError(const Twine &Err) {
  llvm::errs() << "[daemon] error: " << Err << "\n";
  std::exit(StatusInitError);
}

bool detectDaemonArg(int Argc, char **Argv) {
  // `--daemon` must be the first argument.
  return Argc >= 2 && StringRef("--daemon") == Argv[1];
}

void initializeDaemonOptions() {
  static cl::OptionCategory DaemonCategory(
      "Daemon Options", "Options for running tools in daemon mode");

  static cl::opt<bool, true> DaemonModeOpt(
      "daemon", cl::cat(DaemonCategory), cl::ReallyHidden,
      cl::location(DaemonMode), cl::init(false));

  static cl::opt<int, true> DaemonStatusFdOpt(
      "daemon-status-fd",
      cl::desc("File descriptor to which the daemon tool will send status "
               "messages."),
      cl::cat(DaemonCategory), cl::ReallyHidden, cl::location(DaemonStatusFd),
      cl::init(-1));

  static cl::opt<int, true> DaemonStatusHandleOpt(
      "daemon-status-handle",
      cl::desc("Windows file handle to which the daemon tool will send status "
               "messages."),
      cl::cat(DaemonCategory), cl::ReallyHidden,
      cl::location(DaemonStatusHandle), cl::init(-1));
}

/// Utility to read an input stream line-by-line.
class LineReader {
public:
  explicit LineReader(FILE *const File) : File(File) {}

  std::string readLine() {
    std::string Result;
    llvm::raw_string_ostream ResultWriter(Result);

    constexpr size_t BufSize = 512;
    char Buf[BufSize];

    // Read from the file, appending to the result, until a new line character
    // is found.
    while (std::fgets(Buf, BufSize, File)) {
      ResultWriter << Buf;

      if (std::strchr(Buf, '\n'))
        break;
    }

    return Result;
  }

private:
  FILE *File;
};

class DaemonDriver {
public:
  DaemonDriver(LLVMTool &Tool, const int StatusPipeFd)
      : Tool(Tool), StatusPipeWriter(createStatusPipeWriter(StatusPipeFd)) {};

  int run() {
    LineReader StdinReader(stdin);

    // Inform the user that the daemon is ready to receive commands.
    messageOk();

    while (true) {
      const std::string Command = StdinReader.readLine();
      StringRef Remaining = Command;

      if (Remaining.consume_front(CommandRun)) {
        Remaining = Remaining.trim();
        runTool(Remaining);
      } else if (Remaining.consume_front(CommandInputFile)) {
        Remaining = Remaining.trim();
        readInputFromFile(Remaining);
      } else if (Remaining.consume_front(CommandInputString)) {
        Remaining = Remaining.trim();

        // Read number of bytes.
        size_t Len;
        const bool Err = Remaining.consumeInteger(10, Len);
        if (Err) {
          reportCommandError("Expected integer length after " +
                             CommandInputString);
        }
        if (!Remaining.trim().empty()) {
          reportCommandError("Unexpected trailing characters in command: " +
                             Command);
        }
        messageOk();

        readInputFromStdin(Len);
      } else if (Remaining.consume_front(CommandExit)) {
        break;
      } else if (Remaining.consume_front(CommandRedirectStderrToStdout)) {
        if (!Remaining.trim().empty()) {
          reportCommandError("Unexpected trailing characters in command: " +
                             Command);
        }
        RedirectStderrToStdout = true;
      } else {
        reportCommandError("Unexpected command: " + Command);
      }
    }

    return 0;
  }

private:
  static constexpr StringRef CommandRun = "run";
  static constexpr StringRef CommandInputFile = "in.file";
  static constexpr StringRef CommandInputString = "in.str";
  static constexpr StringRef CommandExit = "exit";
  static constexpr StringRef CommandRedirectStderrToStdout =
      "redirect_stderr_to_stdout";

  static constexpr StringRef MessageOk = "ok";
  static constexpr StringRef MessageReturned = "returned";
  static constexpr StringRef MessageError = "error";

  static raw_fd_ostream createStatusPipeWriter(const int Fd) {
    // Only close the status pipe if it is not a standard stream.
    const bool ShouldClose = Fd != STDOUT_FILENO && Fd != STDERR_FILENO;

    return raw_fd_ostream(Fd, ShouldClose, /*unbuffered=*/true);
  }

  [[noreturn]] void reportCommandError(const Twine &Err) {
    StatusPipeWriter << MessageError << ' ' << Err << "\n";
    StatusPipeWriter.flush();
    std::exit(StatusCommandError);
  }

  void messageOk() {
    StatusPipeWriter << MessageOk << "\n";
    StatusPipeWriter.flush();
  }

  void messageReturned(const int ExitCode) {
    StatusPipeWriter << MessageReturned << ' ' << ExitCode << "\n";
    StatusPipeWriter.flush();
  }

  void runTool(const StringRef Command) {
    std::vector<std::string> Args = splitCommandIntoArgs(Command.trim());

    // Convert arguments to C strings, so that they can be passed through
    // `argc`.
    SmallVector<char *, 16> ArgsCStr;
    ArgsCStr.reserve(Args.size());
    for (std::string &Arg : Args) {
      // NB: Since C++11, `std::string` is required to be null-terminated.
      ArgsCStr.push_back(Arg.data());
    }

    // Invoke the tool.
    int ExitCode;
    {
      std::optional<ScopedFileRedirect> StderrRedirect;
      if (RedirectStderrToStdout)
        StderrRedirect.emplace(STDERR_FILENO, STDOUT_FILENO);

      ExitCode = Tool.run(ArgsCStr.size(), ArgsCStr.data(),
                          MemoryBufferRef(ToolInput, "<stdin>"));
    }

    // Reset state for the next invocation.
    Tool.resetState();
    ToolInput.clear();
    RedirectStderrToStdout = false;

    // Make sure the user gets all the output.
    llvm::outs().flush();
    llvm::errs().flush();

    // Inform the user that the command has finished and provide the exit code.
    messageReturned(ExitCode);
  }

  void readInputFromFile(const StringRef Path) {
    const ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
        MemoryBuffer::getFile(Path);

    if (const std::error_code EC = FileOrErr.getError()) {
      reportCommandError("Couldn't open file '" + Path + "': " + EC.message());
    }

    ToolInput = FileOrErr.get()->getMemBufferRef().getBuffer();

    messageOk();
  }

  void readInputFromStdin(const size_t Len) {
    // Read `Len` bytes into `ToolInput`.
    ToolInput.clear();
    ToolInput.resize(Len);
    const size_t Read = fread(ToolInput.data(), sizeof(char), Len, stdin);

    // Make sure the expected number of bytes was read.
    if (Read != Len) {
      reportCommandError("Missing bytes for '" + CommandInputString +
                         "': expected " + Twine(Len) + " got " + Twine(Read));
    }

    messageOk();
  }

  /// Splits a command string into arguments.
  std::vector<std::string> splitCommandIntoArgs(const StringRef Command) {
    std::vector<std::string> Args;
    std::optional<char> OuterQuote; // " or '
    std::string CurrentArgSoFar;    // Argument parsed so far
    char LastChar = '\0';           // Used to detect escape sequences

    for (std::size_t CharIndex = 0; CharIndex < Command.size(); CharIndex++) {
      const char Char = Command[CharIndex];

      if (OuterQuote) {
        // Inside quotes
        if (Char == OuterQuote && LastChar != '\\') {
          OuterQuote.reset();
        } else {
          CurrentArgSoFar.push_back(Char);
        }
      } else {
        // Not inside quotes
        switch (Char) {
        case ' ':
        case '\t':
          if (!CurrentArgSoFar.empty()) {
            Args.push_back(CurrentArgSoFar);
            CurrentArgSoFar.clear();
          }
          break;
        case '\'':
        case '"':
          if (LastChar != '\\') {
            OuterQuote = Char;
          } else {
            CurrentArgSoFar.push_back(Char);
          }
          break;
        default:
          CurrentArgSoFar.push_back(Char);
          break;
        }
      }

      LastChar = Char;
    }
    if (OuterQuote) {
      reportCommandError("Unterminated quotes in command.");
      std::exit(1);
    }
    if (!CurrentArgSoFar.empty()) {
      Args.push_back(CurrentArgSoFar);
    }

    return Args;
  }

  LLVMTool &Tool;
  std::string ToolInput;
  raw_fd_ostream StatusPipeWriter;
  bool RedirectStderrToStdout = false;
};

int runDaemonMode(LLVMTool &Tool, int Argc, char **Argv) {
  sys::ChangeStdinToBinary();
  sys::ChangeStdoutToBinary();
  sys::ChangeStderrToBinary();

  // Parse daemon command line options.
  initializeDaemonOptions();
  cl::ParseCommandLineOptions(Argc, Argv);

  if (DaemonStatusFd < 0 && DaemonStatusHandle < 0) {
    reportInitError("Must provide either `--daemon-status-fd` or "
                    "`--daemon-status-handle (Windows-only)`");
  }
  if (DaemonStatusFd >= 0 && DaemonStatusHandle >= 0) {
    reportInitError("Cannot provide both `--daemon-status-fd` and "
                    "`--daemon-status-handle`");
  }

#ifdef _WIN32
  const int StatusPipeFd = [] {
    if (DaemonStatusHandle >= 0) {
      return _open_osfhandle(DaemonStatusHandle, 0);
    }
    return DaemonStatusFd;
  }();
#else
  if (DaemonStatusHandle >= 0) {
    reportInitError(
        "`--daemon-status-handle` should only be passed on Windows.");
  }
  const int StatusPipeFd = DaemonStatusFd;
#endif

  cl::ResetAllOptionOccurrences();

  DaemonDriver Driver(Tool, StatusPipeFd);
  return Driver.run();
}
} // namespace

LLVM_ABI int llvm::runWithDaemonSupport(LLVMTool &Tool, int Argc, char **Argv) {
  if (detectDaemonArg(Argc, Argv)) {
    return runDaemonMode(Tool, Argc, Argv);
  }

  return Tool.run(Argc, Argv, std::nullopt);
}
