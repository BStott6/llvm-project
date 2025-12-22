// TODO(BStott) file header

#include "llvm/Support/DaemonMode.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <optional>
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

[[noreturn]] static void daemonError(const Twine &Err) {
  llvm::errs() << "[daemon] Error: " << Err << "\n";
  std::exit(1);
}

/// Splits a command string into arguments.
static std::vector<std::string> splitCommandIntoArgs(const StringRef Command) {
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
    daemonError("Unterminated quotes in command");
  }
  if (!CurrentArgSoFar.empty()) {
    Args.push_back(CurrentArgSoFar);
  }

  return Args;
}

LLVM_ABI void llvm::runDaemonMode(ToolInvokeFn InvokeTool) {
  constexpr StringRef CommandExit = "exit";
  constexpr StringRef CommandRun = "run:";
  constexpr StringRef CommandInputFile = "input.file:";
  constexpr StringRef CommandInputStr = "input.str:";

  LineReader StdinReader(stdin);
  std::string ToolInput;

  while (const std::optional<StringRef> CommandOpt = StdinReader.readLine()) {
    StringRef Command = CommandOpt.value();

    if (Command.consume_front(CommandExit)) {
      return;
    }
    if (Command.consume_front(CommandRun)) {
      std::vector<std::string> Args = splitCommandIntoArgs(Command.trim());

      // Convert arguments to C strings, so that they can be passed through
      // `argc`.
      SmallVector<char *, 16> ArgsCStr;
      ArgsCStr.reserve(Args.size());
      for (std::string &Arg : Args) {
        Arg.push_back('\0');
        ArgsCStr.push_back(Arg.data());
      }

      // Invoke the tool itself.
      int ExitCode =
          InvokeTool(static_cast<int>(ArgsCStr.size()), ArgsCStr.data(),
                     MemoryBufferRef(ToolInput, "<stdin>"));

      // Send message on both output streams indicating to stop reading.
      llvm::outs() << "[daemon] End of stdout\n";
      llvm::outs().flush();
      llvm::errs() << "[daemon] End of stderr\n";
      llvm::errs().flush();

      // Send message on `stdout` indicating the exit code.
      llvm::outs() << "[daemon] Exit code: " << ExitCode << "\n";
      llvm::outs().flush();

      // Reset the tool input for the next invocation.
      ToolInput.clear();
    } else if (Command.consume_front(CommandInputFile)) {
      const StringRef FileName = Command.trim();

      ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
          MemoryBuffer::getFile(FileName);

      if (std::error_code EC = FileOrErr.getError()) {
        daemonError("Couldn't open file '" + FileName + "':" + EC.message());
      }

      ToolInput = FileOrErr.get()->getMemBufferRef().getBuffer();
    } else if (Command.consume_front(CommandInputStr)) {
      // Read number of bytes.
      size_t Len;
      const bool Err = Command.consumeInteger(10, Len);
      if (Err) {
        daemonError("Expected integer length after " + CommandInputStr);
      }
      if (!Command.trim().empty()) {
        daemonError("Unexpected trailing characters in command: " + Command);
      }

      // Read `Len` bytes into `ToolInput`.
      ToolInput.clear();
      ToolInput.resize(Len);
      size_t Read = fread(ToolInput.data(), sizeof(char), Len, stdin);

      // Make sure the expected number of bytes was read.
      if (Read != Len) {
        daemonError("Missing bytes for '" + CommandInputStr + "': expected " +
                    Twine(Len) + " got " + Twine(Read));
      }
    } else {
      daemonError("Unexpected command: " + Command);
    }
  }
}
