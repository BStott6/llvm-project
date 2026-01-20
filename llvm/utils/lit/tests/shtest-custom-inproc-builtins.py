## This test provides some custom in-process built-ins via the lit.cfg, and
## verifies that their output can be redirected correctly.
#
# RUN: not %{lit} -v %{inputs}/shtest-custom-inproc-builtins \
# RUN: | FileCheck -match-full-lines %s
# END.

# CHECK: FAIL: shtest-custom-inproc-builtins :: env-inproc-builtin.txt ({{[^)]*}})
# CHECK: {{\*+}} TEST 'shtest-custom-inproc-builtins :: env-inproc-builtin.txt' FAILED {{\*+}}
# CHECK-NEXT: Exit Code: 127
# CHECK-EMPTY:
# CHECK-NEXT: Command Output (stdout):
# CHECK-NEXT: --
# CHECK-NEXT: # RUN: at line 2
# CHECK-NEXT: env custom_echo "hello world"
# CHECK-NEXT: # executed command: env custom_echo 'hello world'
# CHECK-NEXT: # .---command stderr------------
# CHECK-NEXT: # | Error: 'env' cannot call 'custom_echo'
# CHECK-NEXT: # `-----------------------------
# CHECK-NEXT: # error: command failed with exit status: 127

# CHECK: FAIL: shtest-custom-inproc-builtins :: not-crash-inproc-builtin.txt ({{[^)]*}})
# CHECK: {{\*+}} TEST 'shtest-custom-inproc-builtins :: not-crash-inproc-builtin.txt' FAILED {{\*+}}
# CHECK-NEXT: Exit Code: 127
# CHECK-EMPTY:
# CHECK-NEXT: Command Output (stdout):
# CHECK-NEXT: --
# CHECK-NEXT: # RUN: at line 2
# CHECK-NEXT: not --crash custom_echo "hello world"
# CHECK-NEXT: # executed command: not --crash custom_echo 'hello world'
# CHECK-NEXT: # .---command stderr------------
# CHECK-NEXT: # | Error: 'not --crash' cannot call 'custom_echo'
# CHECK-NEXT: # `-----------------------------
# CHECK-NEXT: # error: command failed with exit status: 127

# CHECK: PASS: shtest-custom-inproc-builtins :: use-custom-inproc-builtins.txt ({{[^)]*}})

# CHECK: Passed: 1 ({{[^)]*}})
# CHECK: Failed: 2 ({{[^)]*}})
