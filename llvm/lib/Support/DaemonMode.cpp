#include "llvm/Support/DaemonMode.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace llvm;

static std::vector<std::string> splitCommandIntoArgs(const StringRef Command) {
  std::vector<std::string> Result;
  std::optional<char> OuterQuote; // " or '
  std::string CurrentArgSoFar; // Argument parsed so far 
  char LastChar = '\0'; // Used to detect escape sequences

  for (std::size_t CharIndex = 0; CharIndex < Command.size(); CharIndex++) {
    const auto Char = Command[CharIndex];

    if (!OuterQuote.has_value()) {
      // Not inside quotes
      switch (Char) {
      case ' ':
      case '\t': 
        if (!CurrentArgSoFar.empty()) {
          Result.push_back(CurrentArgSoFar);
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
    } else {
      // Inside quotes
      if (Char == OuterQuote && LastChar != '\\') {
        OuterQuote.reset();
      } else {
        CurrentArgSoFar.push_back(Char);
      }
    }

    LastChar = Char;
  }
  if (!CurrentArgSoFar.empty()) {
    Result.push_back(CurrentArgSoFar);
  }

  return Result;
}

static std::string readFileToString(StringRef Filename) {
  std::ifstream In(Filename.str());
  std::stringstream Buffer;
  Buffer << In.rdbuf();
  return Buffer.str();
}

LLVM_ABI int llvm::runDaemonMode(
    std::function<int(int, char **, DaemonInvocationContext)> F) {
  constexpr StringRef CommandPrefixRun = "run ";
  constexpr StringRef CommandPrefixInputBytes = "in.bytes ";
  constexpr StringRef CommandPrefixInputFilename = "in.file ";

  std::string Command;
  std::string InputContent;

  while (std::getline(std::cin, Command)) {
    StringRef CommandRef = Command;

    if (CommandRef.consume_front(CommandPrefixRun)) {
      const std::string DefaultInput = "-";

      // Stop option values from being carried over from the the previous run
      cl::ResetAllOptionOccurrences();

      const auto Args = splitCommandIntoArgs(CommandRef);

      // Convert args to C strings
      std::vector<std::unique_ptr<char[]>>
          ArgsCStrOwners; // Unique pointers owning ArgsCStr
      std::vector<char *> ArgsCStr;
      ArgsCStrOwners.reserve(Args.size());
      ArgsCStr.reserve(Args.size());
      for (const auto Arg : Args) {
        char *Dest =
            ArgsCStrOwners.emplace_back(new char[Arg.size() + 1]).get();
        std::copy(Arg.begin(), Arg.end(), Dest);
        Dest[Arg.size()] = '\0';
        ArgsCStr.push_back(Dest);
      }

      // Invoke `F`
      const DaemonInvocationContext DIC{InputContent};
      const int ReturnCode = F(Args.size(), ArgsCStr.data(), DIC);

      // Print statistics from this run and reset statistics for the next run
      if (AreStatisticsEnabled()) {
        PrintStatistics(llvm::errs()); // FIXME Not the correct stream if custom
                                       // statistics file is provided, solution?
        ResetStatistics();
      }

      llvm::outs().flush();
      llvm::errs().flush();

      llvm::outs() << "[daemon] Task finished with code " << ReturnCode << "\n";
      llvm::outs().flush();

      // Reset input content
      InputContent = "";
    } else if (CommandRef.consume_front(CommandPrefixInputBytes)) {
      int NumBytes;
      bool ParseError = CommandRef.consumeInteger(10, NumBytes);
      if (ParseError) {
        errs() << "expected integer (number of bytes)\n";
        return 1;
      }

      InputContent.clear();
      InputContent.resize(NumBytes);
      std::cin.read(InputContent.data(), NumBytes);
    } else if (CommandRef.consume_front(CommandPrefixInputFilename)) {
      InputContent = readFileToString(CommandRef.trim());
    } else if (CommandRef == "q") {
      break;
    }
  }

  return 0;
}
