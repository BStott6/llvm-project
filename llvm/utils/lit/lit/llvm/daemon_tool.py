import functools
import os
import subprocess
from threading import Thread
from typing import Any, Callable, Optional, Tuple
try:
    from queue import Queue, Empty
except ImportError:
    from Queue import Queue, Empty

from lit.InprocBuiltins import InprocBuiltin, InprocBuiltinIO
from lit.ShCommands import Command
from lit.ShellEnvironment import ShellEnvironment, InternalShellError, kIsWindows, updateEnv
from lit.llvm.config import LLVMConfig
from lit.llvm.subst import FindTool


debug = False
"""
Output a trace of data sent to and received from the daemon process.
"""


if kIsWindows:
    import msvcrt

    # Required for `set_blocking`.
    from ctypes import windll, byref, wintypes, GetLastError, WinError, POINTER
    from ctypes.wintypes import HANDLE, DWORD, BOOL


def async_read_status_pipe(daemon):
    """
    Runs in the background, reading from the status pipe from the daemon
    process and writing it to a queue. We use a queue so that the pipe can be 
    read with blocking (via `get`) and without (via `get_nowait`)
    """

    while daemon.is_alive():
        line = daemon.status_pipe.readline()
        if debug:
            print("from status pipe:", line)
        daemon.status_pipe_queue.put(line)
    daemon.status_pipe_queue.put(b"")


def set_blocking(pipefd: int, blocking: bool):
    """
    Cross-platform equivalent for `os.set_blocking`.
    """

    if kIsWindows:
        # `os.set_blocking` is not available on Windows.
        # The following solution is from the StackOverflow answer
        # https://stackoverflow.com/a/34504971 by user anatoly techtonik.

        LPDWORD = POINTER(DWORD)
        PIPE_WAIT = wintypes.DWORD(0x00000000)
        PIPE_NOWAIT = wintypes.DWORD(0x00000001)
        ERROR_NO_DATA = 232

        SetNamedPipeHandleState = windll.kernel32.SetNamedPipeHandleState
        SetNamedPipeHandleState.argtypes = [HANDLE, LPDWORD, LPDWORD, LPDWORD]
        SetNamedPipeHandleState.restype = BOOL

        h = msvcrt.get_osfhandle(pipefd)

        res = windll.kernel32.SetNamedPipeHandleState(
            h,
            byref(PIPE_WAIT if blocking else PIPE_NOWAIT),
            None,
            None,
        )
        if res == 0:
            raise WinError()
    else:
        os.set_blocking(pipefd, blocking)


def quote_args(args: list[str]) -> list[str]:
    def quote(arg: str):
        if " " in arg:
            return f"'{arg}'"
        else:
            return arg

    return [quote(arg) for arg in args]


class DaemonError(Exception):
    """
    Exception raised when the daemon tool sends an error message.
    """

    def __init__(self, message: str):
        super().__init__()
        self.message = message.removeprefix("error ")

    def __str__(self) -> str:
        return f"Got error from daemon: {self.message}"


class UnexpectedDaemonOutput(Exception):
    """
    Exception raised when the daemon tool sends an unexpected message.
    """

    def __init__(self, message: bytes):
        super().__init__()
        self.message = message

    def __str__(self) -> str:
        return f"Unexpected message from daemon: {self.message}"


class DaemonExited(Exception):
    """
    Exception raised when the daemon exits unexpectedly.
    """

    exit_code: int
    stdout: bytes
    stderr: bytes

    def __init__(self, exit_code: int, stdout: bytes, stderr: bytes):
        super().__init__()
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self) -> str:
        return "Daemon exited unexpectedly with code {}.\nstdout:\n{}\nstderr:\n{}\n".format(self.exit_code, self.stdout, self.stderr)


class DaemonTool:
    executable_path: str
    daemon_proc: Optional[subprocess.Popen]
    status_pipe: Any
    status_pipe_queue: Queue
    status_pipe_reader_thread: Optional[Thread]

    def __init__(self, executable_path: str):
        self.executable_path = executable_path
        self.daemon_proc = None
        self.status_pipe = None
        self.status_pipe_queue = Queue()
        self.status_pipe_reader_thread = None

    def is_alive(self):
        if not self.daemon_proc:
            return False
        if not isinstance(self.daemon_proc, subprocess.Popen):
            return False

        return self.daemon_proc.poll() is None

    def start_daemon(self):
        assert not self.is_alive(), "start_daemon called but daemon is already alive."

        # Close the old status pipe.
        if self.status_pipe:
            self.status_pipe.close()

        # Kill the old status pipe reading thread.
        if self.status_pipe_reader_thread:
            self.status_pipe_reader_thread.join()

        # Clear the status pipe queue, to avoid issues caused by lingering
        # messages.
        self.status_pipe_queue = Queue()

        # Create a new status pipe for the daemon process.
        # This will be used by the daemon to communicate its status, including
        # exit codes.
        status_pipe_reader, status_pipe_writer = os.pipe()

        # Make sure that the write end of the status pipe gets inherited
        # by the daemon.
        os.set_inheritable(status_pipe_writer, True)
        if kIsWindows:
            status_pipe_handle = msvcrt.get_osfhandle(status_pipe_writer)
            os.set_handle_inheritable(status_pipe_handle, True)

        args = [
            self.executable_path,
            "--daemon",
        ]
        # On Windows, only the file handle (not the file descriptor) is
        # inherited.
        if kIsWindows:
            args.append(f"--daemon-status-handle={status_pipe_handle}")
            startupinfo = subprocess.STARTUPINFO(
                lpAttributeList={"handle_list": [status_pipe_handle]},
            )
            pass_fds = None
        else:
            args.append(f"--daemon-status-fd={status_pipe_writer}")
            startupinfo = None
            pass_fds = [status_pipe_writer]

        self.daemon_proc = subprocess.Popen(
            args=args,
            text=False,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=pass_fds,
            startupinfo=startupinfo,
        )

        # Close our status pipe writer, as we only read from the pipe.
        os.close(status_pipe_writer)

        set_blocking(self.daemon_proc.stdout.fileno(), False)
        set_blocking(self.daemon_proc.stderr.fileno(), False)

        # Start the status pipe reader thread.
        self.status_pipe = open(status_pipe_reader, "rb")
        self.status_pipe_reader_thread = Thread(
            target=async_read_status_pipe,
            args=[self],
            daemon=True,
        )
        self.status_pipe_reader_thread.start()

        # Check initialization status.
        self.check_ok()

    def invoke(
        self,
        args: list[str],
        stdin: Any,
        stdout: Any,
        stderr: Any,
    ) -> int:
        # Ensure the daemon is alive.
        if not self.is_alive():
            self.start_daemon()

        # Set the input for the tool.
        if hasattr(stdin, "name") and isinstance(stdin.name, str):
            # If the input is a named file, we provide it to the daemon via the
            # "input_file" command.
            self.command_input_file(stdin.name)
        else:
            # Otherwise, read the input and provide it via stdin.
            self.command_input_string(stdin.read())

        # If stdout and stderr are the same stream (which will be the case if
        # stderr is redirected to stdout), inform the daemon to send stderr
        # over stdout. We do this to make sure that the order of output is
        # preserved.
        if stderr == stdout:
            self.send_command("redirect_stderr_to_stdout")

        # Run the tool.
        (exit_code, stdout_bytes, stderr_bytes) = self.command_run(args)

        if stderr == stdout:
            stdout.write(stdout_bytes)
            stdout.flush()
        else:
            stdout.write(stdout_bytes)
            stdout.flush()
            stderr.write(stderr_bytes)
            stderr.flush()

        return exit_code

    def command_run(self, args: list[str]) -> Tuple[int, bytes, bytes]:
        command_str = " ".join(quote_args(args))
        self.send_command(f"run {command_str}")

        # Wait for a message on the status pipe, indicating the result of the
        # command, while continually reading all output from the daemon on
        # its output streams. It is important to read the output continually
        # rather than reading it all at the end to avoid deadlock if the pipe
        # becomes full.
        message = b""
        stdout = b""
        stderr = b""
        while self.is_alive():
            # Read output from stdout and stderr so far.
            stdout += self.read_output_so_far(self.daemon_proc.stdout)
            stderr += self.read_output_so_far(self.daemon_proc.stderr)

            # Check for a message from the status pipe, indicating that the
            # task has finished.
            try:
                message = self.status_pipe_queue.get_nowait()
                break
            except Empty:
                continue

        # Make sure to read the remainder of the bytes in the output streams.
        stdout += self.read_output_so_far(self.daemon_proc.stdout)
        stderr += self.read_output_so_far(self.daemon_proc.stderr)

        # Check that the message indicates that the task was completed.
        try:
            self.check_message(
                message,
                lambda message: message.startswith(b"returned"),
            )

            # The exit code is stored in the message.
            exit_code = int(message.removeprefix(b"returned").strip())
            return (exit_code, stdout, stderr)
        except DaemonExited as e:
            # The daemon exited during execution of the task, indicating that
            # the LLVM code crashed or otherwise called `exit`.
            # However the LLVM code exited, the exit code returned by the daemon
            # process is the same as the code that the tool would have returned
            # if run separately, so this is correct to use as the exit code for
            # the test.
            stdout += e.stdout
            stderr += e.stderr
            return (e.exit_code, stdout, stderr)

    def read_output_so_far(self, pipe: Any) -> bytes:
        """
        Read all of the bytes currently in the pipe, which must be in non-
        blocking mode.
        """

        output = b""
        while self.is_alive():
            chunk = pipe.read()
            if not chunk:
                break
            output += chunk

        return output

    def command_input_file(self, path: str):
        self.send_command(f"in.file {path}")
        self.check_ok()

    def command_input_string(self, s: bytes):
        self.send_command(f"in.str {len(s)}")
        self.check_ok()
        self.daemon_proc.stdin.write(s)
        self.daemon_proc.stdin.flush()
        self.check_ok()

    def send_command(self, command: str):
        if debug:
            print("sending command", command)
        self.daemon_proc.stdin.write(f"{command}\n".encode())
        self.daemon_proc.stdin.flush()

    def check_ok(self):
        """
        Read input from the daemon.
        If it sends "ok", do nothing. If it sends "error", raise the error.
        If it sends anything else, raise `UnexpectedDaemonOutput`.
        This will hang if the daemon does not send any output.
        """

        message = self.status_pipe_queue.get()
        self.check_message(message, lambda message: message == b"ok")

    def check_message(
        self,
        message: Optional[bytes],
        predicate: Callable[[bytes], bool],
    ):
        """
        Given a message read from the daemon's status pipe, checks that the
        message matches the predicate. Otherwise, raises the appropriate
        exception (DaemonError, DaemonExited, UnexpectedDaemonOutput)
        """

        if not message:
            # On Windows, we must change these streams back to blocking mode
            # for the output to be captured by `communicate()`.
            set_blocking(self.daemon_proc.stdout.fileno(), True)
            set_blocking(self.daemon_proc.stderr.fileno(), True)

            stdout, stderr = self.daemon_proc.communicate()
            raise DaemonExited(self.daemon_proc.returncode, stdout, stderr)

        if predicate(message.strip()):
            return
        elif message.startswith(b"error"):
            raise DaemonError(message.decode())
        else:
            raise UnexpectedDaemonOutput(message)


daemons: dict[str, DaemonTool] = {}


def invoke_llvm_daemon_tool(
    executable_path: str,
    cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO
):
    """
    Function called by the in-process builtins that are invoking daemon tools.
    """

    # Find the daemon corresponding to this tool executable, or create one.
    daemon: DaemonTool
    if executable_path in daemons.keys():
        daemon = daemons[executable_path]
    else:
        daemon = DaemonTool(executable_path)
        daemons[executable_path] = daemon

    # Invoke the daemon.
    return daemon.invoke(args, io.stdin, io.stdout, io.stderr)


def get_llvm_daemon_inproc_builtin(tool_name: str, config: LLVMConfig, search_dirs: list[str]) -> InprocBuiltin:
    # Find the tool executable in the search directories.
    tool_path = FindTool(tool_name).resolve(
        config,
        os.pathsep.join(search_dirs),
    )
    assert tool_path, f"Could not find {tool_name} in {search_dirs}"

    return InprocBuiltin(
        functools.partial(invoke_llvm_daemon_tool, tool_path),
        fallback=tool_path,
    )
