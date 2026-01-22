============================
Running Tools in Daemon Mode
============================

.. contents::
   :local:

Adding Daemon Support to a Tool
===============================

First, the tool's ``main`` function must be refactored to use the common `LLVMTool`
interface. This has two functions, ``run`` and ``resetState``. The majority of the
body of main (everything aside from ``InitLLVM`` and other one-time initialization)
must be moved to ``run``. This is the function that is called by the daemon to
run the tool given some input and arguments. The handling of standard input
must also be reworked so that if the ``StdinOverride`` argument of ``run`` is not
``nullopt``, the provided buffer is treated as if it were the contents of ``stdin``.
This is how the daemon provides input to the tool, as the input cannot be read from stdin
normally because the pipe never closes.

``resetState`` must also be implemented, which is responsible for resetting any
application state, for example command line options, statistics and debug counters,
for the next invocation. This is only called in daemon mode. Any persistent state
which may affect the output and is not reset by ``resetState`` will break the daemon
testing.

Finally, the contents of main that were refactored into ``run`` can be replaced by
``runWithDaemonSupport(Tool)``, which will detect the daemon argument and either
run the tool in daemon mode or run it normally by deferring to ``run``.

Daemon IPC Protocol
===================

The communication protocol between the daemon and the Lit tester uses four pipes:
the daemon's ``stdin``, ``stdout`` and ``stderr`` and another pipe, called the
`status pipe`, for the daemon to communicate status messages to Lit.

When the daemon first starts it will send an ``ok`` message to indicate that it
has initialized correctly and is ready to receive commands.

The daemon accepts `commands` on ``stdin``. Commands must be separated by a Unix
newline. The following commands are accepted:

#. ``run``: This command is followed by a series of arguments, separated by spaces
   and escaped by quotes. Upon receiving this command, the daemon will run the
   tool, whose output will be sent on ``stdout`` and ``stderr`` as usual. When
   the tool returns, a ``returned`` message is sent on the status pipe
   indicating the exit code.
#. ``in.file``: This command is followed by a file name. It tells the daemon to
   use the content of the file as the standard input for the next ``run``. If
   the file is loaded successfully, the daemon will send an ``ok`` message.
#. ``in.str``: This command is followed by an integer indicating a number of
   bytes. The daemon will read this many bytes from ``stdin`` and use this as
   the standard input for the next ``run``. An ``ok`` message is sent when the
   daemon finishes reading ``stdin``.
#. ``redirect_stderr_to_stdout``: This causes writes to ``stderr`` to be directed
   through ``stdout`` for the next ``run``. This is needed so that the order of
   content is preserved when stderr is redirected to stdout.
#. ``exit``: The daemon will exit.

After a command, the daemon may send the following `messages` along the status
pipe:
#. ``ok``: For ``in.file`` and ``in.str``, this indicates that the input was
   read successfully. This is also sent when the daemon is started.
#. ``error``: This is sent when the daemon encounters an error, for example a
   badly formed command. The daemon will exit after encountering an error. The
   managing process should re-raise the error, as this indicates incorrect use
   of the daemon. The message is followed by a string describing the error.
#. ``returned``. This is sent when a task finishes executing. The message is
   followed by an integer representing the exit code from the task.

If the daemon exits unexpectedly while running the tool, this means that the
tool itself caused the process to exit. The exit code from the daemon should be
taken as the exit code for the task, and the daemon should be restarted.

Each command and message has a space before its argument. An example exchange
may look like:
#. Command: ``in.file llvm/test/Transforms/InstCombine/range-check.ll``
#. Message: ``ok``
#. Command: ``run opt -S -passes=instcombine``
#. Message: ``returned 0``
#. Command: ``bad command``
#. Message: ``error Unexpected command: bad command``

Running a Daemon
================

Tools are invoked in daemon mode by passing ``--daemon`` as the first command line
argument. Additionally, ``--daemon-status-fd`` or ``--daemon-status-handle``
(Windows-only) must be provided to set the file descriptor or Windows file
handle on which the status messages are sent. These must be a valid file
descriptor or handle inherited from the parent process. You can also pass
``--daemon-status-fd=1`` to see the status messages on ``stdout``, which is
useful when testing a daemon from the command line.
