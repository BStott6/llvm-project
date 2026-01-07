// TODO(BStott) file header

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

using namespace llvm;

/// Utility to read an input stream line-by-line, which doesn't seem to be
/// supported by `MemoryBuffer`.
class LineReader {
public:
  LineReader(FILE *File) : File(File) {}

  ~LineReader() {
    if (Buf)
      free(Buf);
  }

  std::optional<StringRef> readLine() {
    ssize_t ReadBytes = getline(&Buf, &BufLen, File);

    if (ReadBytes == -1)
      return std::nullopt;

    return StringRef(Buf, static_cast<size_t>(ReadBytes));
  }

private:
  FILE *File;
  char *Buf = nullptr;
  size_t BufLen = 0;
};

[[noreturn]] static void reportError(const Twine &Err) {
  llvm::errs() << "[daemon] Error: " << Err << "\n";
  std::exit(1);
}

class DaemonDriver {
public:
  DaemonDriver(ToolInvokeFn InvokeTool, ArrayRef<const char *> Args)
      : InvokeTool(InvokeTool),
        StatusPipeWriter(createStatusPipeWriter(Args)) {};

  int run() {
    LineReader StdinReader(stdin);
    

    return 0;
  }

private:
  static raw_fd_ostream createStatusPipeWriter(ArrayRef<const char *> Args) {
    int Fd;

    if (Args.size() < 3) {
      // By default, send messages via stdout.
      Fd = STDOUT_FILENO;
    } else {
      bool Err = StringRef(Args[2]).consumeInteger(10, Fd);

      if (Err) {
        reportError("Failed to parse file descriptor from second argument.");
      }

#ifdef _WIN32
      // Convert Windows file handle to CRT file descriptor.
      Fd = _open_osfhandle(Fd);
#endif
    }

    return raw_fd_ostream(Fd, /*shouldClose=*/Fd != STDOUT_FILENO, /*unbuffered=*/true);
  }

  /// Splits a command string into arguments.
  static std::vector<std::string>
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
      reportError("Unterminated quotes in command");
    }
    if (!CurrentArgSoFar.empty()) {
      Args.push_back(CurrentArgSoFar);
    }

    return Args;
  }

  ToolInvokeFn InvokeTool;
  raw_fd_ostream StatusPipeWriter;
};

LLVM_ABI int llvm::daemonMain(ToolInvokeFn InvokeTool,
                              ArrayRef<const char *> Args) {
  DaemonDriver Driver(InvokeTool, Args);
  return Driver.run();
}
