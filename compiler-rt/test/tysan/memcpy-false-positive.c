// RUN: %clang_tysan -O0 %s -o %t && %run %t >%t.out 2>&1
// RUN: FileCheck %s --allow-empty < %t.out
// CHECK-NOT: ERROR: TypeSanitizer: type-aliasing-violation

#include <string.h>

int main(void) {
  int i = 42;
  float f = 1.0f;
  memcpy(&i, &f, sizeof i);
  i = 1;
}
