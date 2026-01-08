import os
import subprocess
from threading import Thread
from typing import Any, Optional, Tuple
try:
    from queue import Queue, Empty
except ImportError:
    from Queue import Queue, Empty

from lit.InprocBuiltins import InprocBuiltinIO
from lit.ShCommands import Command
from lit.ShellEnvironment import ShellEnvironment, InternalShellError, kIsWindows, updateEnv


def async_enqueue_output(daemon_tool, stream, queue):
    """
    Runs in a background thread, enqueueing bytes read from `stream` into `queue`.
    This is required so we can read the stream content so far without
    blocking in the main thread.
    """

    while daemon_tool.is_alive():
        # NB: daemon always sends a new line after the output, so readline 
        # will never miss any output.
        line = stream.readline()
        queue.put(line)


class DaemonError(Exception):
    """
    Exception raised when the daemon tool sends an error message.
    """

    def __init__(self, message: str):
        message = message.removeprefix("error:")

        super().__init__(message)
        self.message = message

    def __str__(self) -> str:
        return f"Got error from daemon: {self.message}"


class UnexpectedDaemonOutput(Exception):
    """
    Exception raised when the daemon tool sends an unexpected message.
    """

    def __init__(self, message: str):
        super().__init__(message)
        self.message = message

    def __str__(self) -> str:
        return f"Unexpected message from daemon: {self.message}"


class DaemonTool:
    executable_path: str
    daemon_proc: Optional[subprocess.Popen]
    status_pipe_reader: Any
    stdout_queue: Queue
    stdout_enqueueing_thread: Optional[Thread]
    stderr_queue: Queue
    stderr_enqueueing_thread: Optional[Thread]

    def __init__(self, executable_path: str):
        self.executable_path = executable_path
        self.daemon_proc = None
        self.status_pipe_reader = None
        self.stdout_queue = Queue()
        self.stdout_enqueueing_thread = None
        self.stderr_queue = Queue()
        self.stderr_enqueueing_thread = None

    def is_alive(self):
        if not self.daemon_proc:
            return False
        if not isinstance(self.daemon_proc, subprocess.Popen):
            return False

        return self.daemon_proc.poll() is None

    def start_daemon(self):
        assert not self.is_alive(), "start_daemon called but daemon is already alive."

        # Close the old status pipe.
        if self.status_pipe_reader:
            self.status_pipe_reader.close()

        # Close the old output enqueueing threads.
        if self.stdout_enqueueing_thread:
            self.stdout_enqueueing_thread.join()
        if self.stderr_enqueueing_thread:
            self.stderr_enqueueing_thread.join()

        # Create a new status pipe for the daemon process.
        # This will be used by the daemon to communicate its status, including
        # exit codes.
        status_pipe_reader, status_pipe_writer = os.pipe()
        self.status_pipe_reader = open(status_pipe_reader, "rb")

        # Make sure that the write end of the status pipe gets inherited
        # by the daemon.
        os.set_inheritable(status_pipe_writer, True)

        self.daemon_proc = subprocess.Popen(
            args=[
                self.executable_path,
                "--daemon-mode",
                str(status_pipe_writer),
            ],
            text=False,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=[status_pipe_writer],
        )

        # Close our status pipe writer, as we only read from the pipe.
        os.close(status_pipe_writer)
        
        # Start the stdout and stderr enqueueing threads, so we can query the
        # output gathered so far without blocking.
        self.stdout_enqueueing_thread = Thread(
            target=async_enqueue_output,
            args=[self, self.daemon_proc.stdout, self.stdout_queue],
            daemon=True,
        )
        self.stdout_enqueueing_thread.start()

        self.stderr_enqueueing_thread = Thread(
            target=async_enqueue_output,
            args=[self, self.daemon_proc.stderr, self.stderr_queue],
            daemon=True,
        )
        self.stderr_enqueueing_thread.start()

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

        # Run the tool.
        (exit_code, stdout_bytes, stderr_bytes) = self.command_run(args)

        stdout.write(stdout_bytes)
        stdout.flush()
        stderr.write(stderr_bytes)
        stderr.flush()

        return exit_code

    def command_run(self, args: list[str]) -> Tuple[int, bytes, bytes]:
        cmd = " ".join(args)
        self.daemon_proc.stdin.write(f"run:{cmd}\n".encode())
        self.daemon_proc.stdin.flush()

        # Wait for status report.
        assert self.status_pipe_reader
        message = self.status_pipe_reader.readline()
        if message.startswith(b"finished"):
            # Command finished successfully.

            # Parse return code from the message
            exit_code = int(message.split(b":")[1].strip())

            # Read stdout and stderr from the queues.
            stdout = b""
            while True:
                try:
                    stdout += self.stdout_queue.get_nowait()
                except Empty:
                    break
            stderr = b""
            while True:
                try:
                    stderr += self.stderr_queue.get_nowait()
                except Empty:
                    break

            return (exit_code, stdout, stderr)

        elif message.startswith(b"error"):
            raise DaemonError(message.decode())
        elif not message:
            # Daemon exited while running the tool, meaning that the tool
            # being tested exited. This means that the daemon's return code
            # is the return code for the invocation.
            (stdout, stderr) = self.daemon_proc.communicate()
            return (self.daemon_proc.returncode, stdout, stderr)

        raise UnexpectedDaemonOutput(message.decode())

    def command_input_file(self, path: str):
        self.daemon_proc.stdin.write(f"input_file:{path}\n".encode())
        self.daemon_proc.stdin.flush()
        self.check_ok()

    def command_input_string(self, s: bytes):
        self.daemon_proc.stdin.write(f"input_string:{len(s)}\n".encode())
        self.daemon_proc.stdin.flush()
        self.check_ok()
        self.daemon_proc.stdin.write(s)
        self.daemon_proc.stdin.flush()
        self.check_ok()

    def check_ok(self):
        """
        Read input from the daemon.
        If it sends "ok", do nothing. If it sends "error", raise the error.
        If it sends anything else, raise `UnexpectedDaemonOutput`.
        This will hang if the daemon does not send any output.
        """

        assert self.status_pipe_reader
        message = self.status_pipe_reader.readline()
        if message == b"ok\n":
            return
        elif message.startswith(b"error"):
            raise DaemonError(message.decode())
        elif not message:
            (stdout, stderr) = self.daemon_proc.communicate()
            raise RuntimeError(
                "Daemon exited unexpectedly with code {}.\nstdout:\n{}\nstderr:\n{}\n"
                    .format(self.daemon_proc.returncode, stdout, stderr)
            )

        raise UnexpectedDaemonOutput(message.decode())


daemons: dict[str, DaemonTool] = {}


def invoke_llvm_daemon_tool(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO):
    executable_path = os.path.abspath(args[0])

    # Find the daemon corresponding to this tool executable, or create one.
    daemon: DaemonTool
    if executable_path in daemons.keys():
        daemon = daemons[executable_path]
    else:
        daemon = DaemonTool(executable_path)
        daemons[executable_path] = daemon

    # Invoke the daemon.
    return daemon.invoke(args, io.stdin, io.stdout, io.stderr)
