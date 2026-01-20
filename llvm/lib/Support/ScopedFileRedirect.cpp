#include "llvm/Support/ScopedFileRedirect.h"

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

using namespace llvm;

llvm::ScopedFileRedirect::ScopedFileRedirect(const int FromFd, const int ToFd)
    : FromFd(FromFd) {
  // Store a copy of the original file so that we can restore the original fd to
  // this copy.
  CopyFd = DUP_FN(FromFd);

  // Close the source fd and reopen it to the target fd.
  DUP2_FN(ToFd, FromFd);
}

llvm::ScopedFileRedirect::~ScopedFileRedirect() {
  if (!Moved) {
    // Close the source fd and reopen it to the original file.
    DUP2_FN(CopyFd, FromFd);

    // Close the copied file descriptor, as it's no longer needed.
    CLOSE_FN(CopyFd);
  }
}
