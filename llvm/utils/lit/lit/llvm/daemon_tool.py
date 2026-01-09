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


def async_read_status_pipe(daemon):
    while daemon.is_alive():
        line = daemon.status_pipe.readline()
        daemon.status_pipe_queue.put(line)
    status_pipe_queue.put(b"")


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
    status_pipe: Any
    status_pipe_queue: Queue
    status_pipe_reader_thread: Thread

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

        # Clear 
        self.status_pipe_queue = Queue()

        # Create a new status pipe for the daemon process.
        # This will be used by the daemon to communicate its status, including
        # exit codes.
        status_pipe_reader, status_pipe_writer = os.pipe()
        self.status_pipe = open(status_pipe_reader, "rb")

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

        os.set_blocking(self.daemon_proc.stdout.fileno(), False)
        os.set_blocking(self.daemon_proc.stderr.fileno(), False)

        # Close our status pipe writer, as we only read from the pipe.
        os.close(status_pipe_writer)

        # Start the status pipe reader thread.
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

        message = b""
        stdout = b""
        stderr = b""
        while (message == b"" or message == None) and self.is_alive():
            # read everything from stdout so far
            stdout_chunk = b""
            while stdout_chunk == b"" and self.is_alive():
                stdout_chunk = self.daemon_proc.stdout.read()
                if stdout_chunk is None:
                    break
                stdout += stdout_chunk

            # read everything from stderr so far
            stderr_chunk = b""
            while stderr_chunk == b"" and self.is_alive():
                stderr_chunk = self.daemon_proc.stderr.read()
                if stderr_chunk is None:
                    break
                stderr += stderr_chunk

            try:
                message = self.status_pipe_queue.get_nowait()
            except Empty:
                continue

        # read last little chunk of stdout
        stdout_chunk = b""
        while stdout_chunk == b"" and self.is_alive():
            stdout_chunk = self.daemon_proc.stdout.read()
            if stdout_chunk is None:
                break
            stdout += stdout_chunk

        # read last little chunk of stderr
        stderr_chunk = b""
        while stderr_chunk == b"" and self.is_alive():
            stderr_chunk = self.daemon_proc.stderr.read()
            if stderr_chunk is None:
                break
            stderr += stderr_chunk

        if message.startswith(b"finished"):
            # Command finished successfully.

            # Parse return code from the message
            exit_code = int(message.split(b":")[1].strip())

            return (exit_code, stdout, stderr)

        elif message.startswith(b"error"):
            raise DaemonError(message.decode())
        elif not message:
            assert not self.is_alive()
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

        message = self.status_pipe_queue.get()
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
