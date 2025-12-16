// RUN: %clang_tysan -O0 -mllvm -tysan-outline-instrumentation=false %s -o %t && %run %t 10 >%t.out 2>&1
// RUN: FileCheck %s < %t.out
// RUN: %clang_tysan -O0 -mllvm -tysan-outline-instrumentation=true %s -o %t && %run %t 10 >%t.out.0 2>&1
// RUN: FileCheck %s < %t.out.0

struct S {
  long x;
  short y;
};

struct Nested {
  int z;
  struct S inner;
  float w;
};

int main(void) {
  struct S a;
  a.x = 1;
  a.y = 2;

  struct S b = a;
  *(float *)&b.x = 1.0f;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type float accesses an existing object of type long
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]
  *(float *)&b.y = 1.0f;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type float accesses an existing object of type short
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]

  struct Nested n;
  n.z = 1;
  n.inner.x = 2;
  n.inner.y = 3;
  n.z = 4;

  struct Nested m = n;

  *(float *)&m.z = 1.0f;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type float accesses an existing object of type int
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]
  *(float *)&m.inner.x = 1.0f;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type float accesses an existing object of type long
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]
  *(float *)&m.inner.y = 1.0f;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type float accesses an existing object of type short
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]
  *(int *)&m.w = 3;
  // CHECK: ERROR: TypeSanitizer: type-aliasing-violation
  // CHECK: WRITE of size 4 at {{.*}} with type int accesses an existing object of type float
  // CHECK: {{#0 0x.* in main .*struct-copy.c:}}[[@LINE-3]]
}
