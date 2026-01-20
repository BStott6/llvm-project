//===- llvm/Support/DaemonDriver.cpp - Scoped file redirection --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides an RAII mechanism to redirect a file descriptor, for
// example a standard stream, to the file pointed to by a different file
// descriptor. The file descriptor is reset to the original file when the object
// is destroyed.
//
//===----------------------------------------------------------------------===//

namespace llvm {
/// RAII mechanism to redirect a file descriptor to the file pointed to by a
/// different file descriptor. The destructor will reset the file descriptor to
/// its original file.
class ScopedFileRedirect {
public:
  /// Redirect `FromFd` to the same file as `ToFd` for the lifetime of this
  /// object.
  ScopedFileRedirect(int FromFd, int ToFd);

  ScopedFileRedirect(ScopedFileRedirect &&Other) {
    *this = Other;
    Other.Moved = true;
  }
  ScopedFileRedirect &operator=(ScopedFileRedirect &&Other) {
    *this = Other;
    Other.Moved = true;
    return *this;
  }

  ~ScopedFileRedirect();

private:
  ScopedFileRedirect(const ScopedFileRedirect &Other) = default;
  ScopedFileRedirect &operator=(const ScopedFileRedirect &Other) = default;

  int FromFd;
  int CopyFd;
  bool Moved = false;
};
} // namespace llvm
