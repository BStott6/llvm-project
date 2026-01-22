//===- llvm/Support/DaemonDriver.cpp - Daemon driver interface --Options.StatusPipe C++ -*-===//
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
#include <cstdio>
#include <cstdlib>
#include <optional>

#if defined _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Windows emulated POSIX functions are prefixed by `_`.
#ifdef _WIN32
#define CLOSE_FN _close
#define DUP_FN _dup
#define DUP2_FN _dup2
#else
#define CLOSE_FN close
#define DUP_FN dup
#define DUP2_FN dup2
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
namespace util {
/// Utility to read an input stream line-by-line.
class LineReader {
public:
  explicit LineReader(FILE *const File) : File(File) {}

  std::string readLine() {
    std::string Result;
    raw_string_ostream ResultWriter(Result);

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

/// RAII mechanism to redirect a file descriptor to the file pointed to by a
/// different file descriptor. The destructor will reset the file descriptor to
/// its original file.
class ScopedFileRedirect {
public:
  /// Redirect `FromFd` to the same file as `ToFd` for the lifetime of this
  /// object.
  ScopedFileRedirect(int FromFd, int ToFd) : FromFd(FromFd) {
    // Store a copy of the original file so that we can restore the original fd
    // to this copy.
    CopyFd = DUP_FN(FromFd);

    // Close the source fd and reopen it to the target fd.
    DUP2_FN(ToFd, FromFd);
  }

  ~ScopedFileRedirect() {
    if (!Moved) {
      // Close the source fd and reopen it to the original file.
      DUP2_FN(CopyFd, FromFd);

      // Close the copied file descriptor, as it's no longer needed.
      CLOSE_FN(CopyFd);
    }
  }

  ScopedFileRedirect(ScopedFileRedirect &&Other) {
    *this = Other;
    Other.Moved = true;
  }
  ScopedFileRedirect &operator=(ScopedFileRedirect &&Other) {
    *this = Other;
    Other.Moved = true;
    return *this;
  }

private:
  ScopedFileRedirect(const ScopedFileRedirect &Other) = default;
  ScopedFileRedirect &operator=(const ScopedFileRedirect &Other) = default;

  int FromFd;
  int CopyFd;
  bool Moved = false;
};
} // namespace util

/// Status code returned if the daemon fails to initialize, for example due to
/// incorrect command line arguments.
constexpr int StatusInitError = 2;
/// Status code returned if the daemon receives a malformed command.
constexpr int StatusCommandError = 3;

/// This should only be used before the status pipe is set up - after,
/// errors are reported to the user via the status pipe.
[[noreturn]] static void reportInitError(const Twine &Err) {
  errs() << "[daemon] Error: " << Err << "\n";
  std::exit(StatusInitError);
}

/// Returns true if ``--daemon`` is passed as the first command line argument.
static bool detectDaemonArg(int Argc, char **Argv) {
  // `--daemon` must be the first argument.
  return Argc >= 2 && StringRef("--daemon") == Argv[1];
}

struct DaemonCommandLineOptions {
  bool DaemonModeEnabled;
  std::string StatusPipe;
};

// Creates the command line options for configuring the daemon. The returned
// reference points to a static struct where the option values will be stored.
static const DaemonCommandLineOptions &initializeDaemonCommandLineOptions() {
  static DaemonCommandLineOptions Options;

  static cl::opt<bool, true> DaemonModeEnabledOpt(
      "daemon", cl::location(Options.DaemonModeEnabled), cl::init(false));

  static cl::opt<std::string, true> StatusPipeOpt(
      "daemon-status-pipe",
      cl::desc("File to which the daemon tool will send status messages. May "
               "be 'path:{filepath}', 'fd:{file descriptor}' or "
               "'handle:{Windows file handle}'"),
      cl::location(Options.StatusPipe), cl::init(""));

  return Options;
}

/// This class implements the daemon driver functionality.
class DaemonDriver {
public:
  DaemonDriver(LLVMTool &Tool, const DaemonCommandLineOptions &Options)
      : Tool(Tool),
        StatusPipeWriter(createStatusPipeWriter(Options.StatusPipe)) {};

  int run() {
    util::LineReader StdinReader(stdin);

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

  static std::unique_ptr<raw_ostream>
  createStatusPipeWriter(StringRef StatusPipeString) {
    // `StatusPipeString` may be:
    // - "path:{file path}"
    // - "fd:{file descriptor}"
    // - "handle:{Windows file handle}" (Windows-only)
    constexpr StringRef ErrorContext = "Parsing option 'daemon-status-pipe': ";

    if (StatusPipeString.consume_front("path:")) {
      std::error_code EC;
      auto Writer =
          std::make_unique<raw_fd_ostream>(StatusPipeString.trim(), EC);
      if (EC) {
        reportInitError(ErrorContext + "Couldn't open file '" +
                        StatusPipeString + "': " + EC.message());
      }
      Writer->SetUnbuffered();
      return Writer;
    }

    int Fd;
    if (StatusPipeString.consume_front("fd:")) {
      bool Err = StatusPipeString.consumeInteger(10, Fd);
      if (Err) {
        reportInitError(ErrorContext + "expected integer "
                                       "after 'fd:'.");
      }
    } else if (StatusPipeString.consume_front("handle:")) {
#ifdef _WIN32
      int Handle;
      bool Err = StatusPipeString.consumeInteger(10, Handle);
      if (Err) {
        reportInitError(ErrorContext +
                        "Parsing option 'daemon-status-pipe': expected integer "
                        "after 'handle:'.");
      }

      Fd = _open_osfhandle(Handle, 0);
#endif
      reportInitError(ErrorContext + "'handle' may only "
                                     "be specified on Windows");
    } else {
      reportInitError(ErrorContext + "Unexpected value : '" +
                      StatusPipeString + "'");
    }

    // Only close the status pipe if it is not a standard stream.
    const bool ShouldClose = Fd != STDOUT_FILENO && Fd != STDERR_FILENO;
    return std::make_unique<raw_fd_ostream>(Fd, ShouldClose,
                                            /*unbuffered=*/true);
  }

  [[noreturn]] void reportCommandError(const Twine &Err) {
    *StatusPipeWriter << MessageError << ' ' << Err << "\n";
    StatusPipeWriter->flush();
    std::exit(StatusCommandError);
  }

  void messageOk() {
    *StatusPipeWriter << MessageOk << "\n";
    StatusPipeWriter->flush();
  }

  void messageReturned(const int ExitCode) {
    *StatusPipeWriter << MessageReturned << ' ' << ExitCode << "\n";
    StatusPipeWriter->flush();
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
      std::optional<util::ScopedFileRedirect> StderrRedirect;
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
    outs().flush();
    errs().flush();

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
    // Ensure stdin is in binary mode to prevent newline translation on Windows
    // - this not only breaks binary files but also muddles the number of
    // characters read.
    sys::ChangeStdinToBinary();

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
  std::unique_ptr<raw_ostream> StatusPipeWriter;
  bool RedirectStderrToStdout = false;
};

static int runDaemonMode(LLVMTool &Tool, int Argc, char **Argv) {
  // Parse daemon command line options.
  const DaemonCommandLineOptions &Options = initializeDaemonCommandLineOptions();
  cl::ParseCommandLineOptions(Argc, Argv);

  DaemonDriver Driver(Tool, Options);
  return Driver.run();
}
} // namespace

LLVM_ABI int llvm::runWithDaemonSupport(LLVMTool &Tool, int Argc, char **Argv) {
  if (detectDaemonArg(Argc, Argv)) {
    return runDaemonMode(Tool, Argc, Argv);
  }

  return Tool.run(Argc, Argv, std::nullopt);
}
