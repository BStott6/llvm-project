## Test piping together in-process builtin commands.
## This test suite has some new in-process builtin registered in the config:
##
## uppercaser: Reads input on stdin, converts the string to uppercase and
## outputs the result on stderr.
## print_out_err: Writes the first argument to stdout, and the second to stderr.

# RUN: %{lit} -v %{inputs}/piped-inproc-builtins | FileCheck %s -match-full-lines
# END.

# CHECK: PASS
