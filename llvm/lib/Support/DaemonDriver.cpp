#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DaemonDriver.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <cstdlib>
#include <optional>

#if defined _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

// Standard stream fileno macros are not defined on Windows.
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

// Windows emulated POSIX functions are prefixed by `_`.
#ifdef _WIN32
# define CLOSE_FN _close
# define DUP_FN _dup
# define DUP2_FN _dup2
#else
# define CLOSE_FN close
# define DUP_FN dup
# define DUP2_FN dup2
#endif

using namespace llvm;

static bool DaemonOptionsInitialized = false;
static bool DaemonMode;
static int DaemonStatusFd;
static int DaemonStatusHandle;

LLVM_ABI void llvm::initializeDaemonOptions() {
  DaemonOptionsInitialized = true;

  static cl::OptionCategory DaemonCategory(
      "Daemon Options", "Options for running tools in daemon mode");

  static cl::opt<bool, true> DaemonModeOpt(
      "daemon", cl::desc("Run this tool in daemon mode"),
      cl::cat(DaemonCategory), cl::ReallyHidden, cl::location(DaemonMode),
      cl::init(false));

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

LLVM_ABI bool llvm::daemonModeEnabled() {
  assert(DaemonOptionsInitialized &&
         "`initializeDaemonOptions` must be called before `daemonModeEnabled`");
  return DaemonMode;
}

/// Utility to read an input stream line-by-line.
/// Can't use `std::cin` because the <iostream> header is forbidden, and can't
/// use `std::ifstream` because there isn't a way to construct one from a file
/// descriptor.
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

/// This should only be used before the status pipe is set up - after,
/// errors are reported to the user via the status pipe.
[[noreturn]] static void reportInitError(const Twine &Err) {
  llvm::errs() << "[daemon] error: " << Err << "\n";
  std::exit(1);
}

class DaemonDriver {
public:
  DaemonDriver(const ToolInvokeFn InvokeTool, const int StatusPipeFd)
      : InvokeTool(InvokeTool),
        StatusPipeWriter(createStatusPipeWriter(StatusPipeFd)) {};

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
          exitWithError("Expected integer length after " + CommandInputString);
        }
        if (!Remaining.trim().empty()) {
          exitWithError("Unexpected trailing characters in command: " + Command);
        }
        messageOk();

        readInputFromStdin(Len);
      } else if (Remaining.consume_front(CommandExit)) {
        break;
      } else if (Remaining.consume_front(CommandRedirectStderrToStdout)) {
        if (!Remaining.trim().empty()) {
          exitWithError("Unexpected trailing characters in command: " + Command);
        }
        redirectStderrToStdout();
      } else {
        exitWithError("Unexpected command: " + Command);
      }
    }

    return 0;
  }

private:
  static constexpr StringRef CommandRun = "run";
  static constexpr StringRef CommandInputFile = "in.file";
  static constexpr StringRef CommandInputString = "in.str";
  static constexpr StringRef CommandExit = "exit";
  static constexpr StringRef CommandRedirectStderrToStdout = "redirect_stderr_to_stdout";

  static constexpr StringRef MessageOk = "ok";
  static constexpr StringRef MessageReturned = "returned";
  static constexpr StringRef MessageError = "error";

  static raw_fd_ostream createStatusPipeWriter(const int Fd) {
    // Only close the status pipe if it is not a standard stream.
    const bool ShouldClose = Fd != STDOUT_FILENO && Fd != STDERR_FILENO;

    return raw_fd_ostream(Fd, ShouldClose, /*unbuffered=*/true);
  }

  [[noreturn]] void exitWithError(const Twine &Err) {
    StatusPipeWriter << MessageError << ' ' << Err << "\n";
    StatusPipeWriter.flush();
    std::exit(1);
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

    // Invoke the tool itself.
    const int ExitCode = InvokeTool(ArgsCStr.size(), ArgsCStr.data(),
                                    MemoryBufferRef(ToolInput, "<stdin>"));

    // Make sure the user gets all the output.
    llvm::outs().flush();
    llvm::errs().flush();

    // Reset state for the next invocation.
    ToolInput.clear();
    if (StderrRedirectedToStdout) {
      resetStderr();
    }

    // Inform the user that the command has finished and provide the exit code.
    messageReturned(ExitCode);
  }

  void readInputFromFile(const StringRef Path) {
    const ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
        MemoryBuffer::getFile(Path);

    if (const std::error_code EC = FileOrErr.getError()) {
      exitWithError("Couldn't open file '" + Path + "': " + EC.message());
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
      exitWithError("Missing bytes for '" + CommandInputString + "': expected " +
                   Twine(Len) + " got " + Twine(Read));
    }

    messageOk();
  }

  /// Redirect the `stderr` stream to point to the same file descriptor as
  /// stdout, duplicating the original file descriptor for stderr so that it
  /// can be reset later.
  void redirectStderrToStdout() {
    if (StderrRedirectedToStdout) {
      exitWithError("Stderr is already redirected to stdout.");
      std::exit(1);
    }

    llvm::errs().flush();

    // Store a copy of the original file so that it can be reset later.
    StderrCopy = DUP_FN(STDERR_FILENO);

    // Close stderr and open it to stdout.
    DUP2_FN(STDOUT_FILENO, STDERR_FILENO);

    StderrRedirectedToStdout = true;
  }

  void resetStderr() {
    assert(StderrRedirectedToStdout && StderrCopy.has_value());

    llvm::errs().flush();

    // Close stderr and open it to the original stderr.
    DUP2_FN(*StderrCopy, STDERR_FILENO);

    // The copied stderr file descriptor is no longer needed.
    CLOSE_FN(*StderrCopy);

    StderrCopy = std::nullopt;
    StderrRedirectedToStdout = false;
  }

  /// Splits a command string into arguments.
  std::vector<std::string>
  splitCommandIntoArgs(const StringRef Command) {
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
      exitWithError("Unterminated quotes in command.");
      std::exit(1);
    }
    if (!CurrentArgSoFar.empty()) {
      Args.push_back(CurrentArgSoFar);
    }

    return Args;
  }

  ToolInvokeFn InvokeTool;
  std::string ToolInput;
  raw_fd_ostream StatusPipeWriter;
  bool StderrRedirectedToStdout = false;
  std::optional<int> StderrCopy;
};

LLVM_ABI int llvm::runDaemonMode(ToolInvokeFn InvokeTool) {

  assert(DaemonOptionsInitialized &&
         "`initializeDaemonOptions` must be called before `runDaemonMode`");

  if (DaemonStatusFd < 0 && DaemonStatusHandle < 0) {
    reportInitError("Must provide either `--daemon-status-fd` or `--daemon-status-handle`");
  }
  if (DaemonStatusFd >= 0 && DaemonStatusHandle >= 0) {
    reportInitError("Cannot provide both `--daemon-status-fd` and `--daemon-status-handle`");
  }

#ifndef _WIN32
  if (DaemonStatusHandle.has_value()) {
    reportInitError("`--daemon-status-handle` should only be passed on Windows.");
  }
  int StatusPipeFd = DaemonStatusFd;
#else
  int StatusPipeFd;
  if (DaemonStatusFd >= 0) {
    StatusPipeFd = DaemonStatusFd;
  } else {
    StatusPipeFd = _open_osfhandle(DaemonStatusHandle, 0);
  }
#endif

  DaemonDriver Driver(InvokeTool, StatusPipeFd);
  return Driver.run();
}
