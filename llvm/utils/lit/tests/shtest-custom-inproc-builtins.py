## This test provides some custom in-process built-ins via the lit.cfg, and
## verifies that their output can be redirected correctly.
#
# RUN: not %{lit} -v %{inputs}/shtest-custom-inproc-builtins \
# RUN: | FileCheck -match-full-lines %s
# END.

# CHECK: FAIL: shtest-custom-inproc-builtins :: illegal-fallback-1.txt ({{.*}})
# CHECK-NEXT: {{\*+}} TEST 'shtest-custom-inproc-builtins :: illegal-fallback-1.txt' FAILED {{\*+}}
# CHECK: Exit Code: 127
# CHECK-EMPTY:
# CHECK-NEXT: Command Output (stdout):
# CHECK-NEXT: --
# CHECK-NEXT: # RUN: at line {{.*}}
# CHECK-NEXT: not not --crash returns_0
# CHECK-NEXT: # executed command: not not --crash returns_0
# CHECK-NEXT: # .---command stderr------------
# CHECK-NEXT: # | Error: 'not --crash' cannot call 'returns_0'
# CHECK-NEXT: # `-----------------------------
# CHECK-NEXT: # error: command failed with exit status: 127

# CHECK: FAIL: shtest-custom-inproc-builtins :: illegal-fallback-2.txt ({{.*}})
# CHECK-NEXT: {{\*+}} TEST 'shtest-custom-inproc-builtins :: illegal-fallback-2.txt' FAILED {{\*+}}
# CHECK: Exit Code: 127
# CHECK-EMPTY:
# CHECK-NEXT: Command Output (stdout):
# CHECK-NEXT: --
# CHECK-NEXT: # RUN: at line {{.*}}
# CHECK-NEXT: env returns_0
# CHECK-NEXT: # executed command: env returns_0
# CHECK-NEXT: # .---command stderr------------
# CHECK-NEXT: # | Error: 'env' cannot call 'returns_0'
# CHECK-NEXT: # `-----------------------------

# CHECK-NEXT: # error: command failed with exit status: 127
# CHECK: PASS: shtest-custom-inproc-builtins :: inproc-builtins-fallback.txt ({{[^)]*}})
# CHECK: PASS: shtest-custom-inproc-builtins :: use-custom-inproc-builtins.txt ({{[^)]*}})

# CHECK: Passed: 2 ({{[^)]*}})
# CHECK: Failed: 2 ({{[^)]*}})
