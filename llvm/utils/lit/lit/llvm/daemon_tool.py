import os
import subprocess
from threading import Thread
from typing import Any, Optional

from lit.InprocBuiltins import InprocBuiltinIO
from lit.ShCommands import Command
from lit.ShellEnvironment import ShellEnvironment, InternalShellError, kIsWindows, updateEnv


class DaemonTool:
    executable_path: str
    daemon_proc: Optional[subprocess.Popen]
    status_pipe_reader_fd: Optional[int]

    def __init__(self, executable_path: str):
        self.executable_path = executable_path
        self.daemon_proc = None 
        self.stderr_enqueueing_thread = None
        self.status_pipe_reader_fd = None

    def is_alive(self):
        if not self.daemon_proc:
            return False
        if not isinstance(self.daemon_proc, subprocess.Popen):
            return False

        return self.daemon_proc.poll() is None

    def start_daemon(self):
        assert not self.is_alive(), "start_daemon called but daemon is already alive."

        # Close the old status pipe.
        if self.status_pipe_reader_fd:
            os.close(self.status_pipe_reader_fd)

        # Close the old stderr enqueueing thread.
        if self.stderr_enqueueing_thread:
            self.stderr_enqueueing_thread.join()

        # Create new status pipe for the daemon process.
        # This will be used by the daemon to communicate its status, including
        # exit codes.
        status_pipe_reader, status_pipe_writer = os.pipe()
        os.set_inheritable(status_pipe_writer, True)

        self.daemon_proc = subprocess.Popen(
            self.executable_path,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=[status_pipe_writer],
        )

        # Close our status pipe writer (we only need the read end.)
        os.close(status_pipe_writer)
        self.status_pipe_reader_fd = status_pipe_reader

    def invoke(
        self,
        args: list[str],
        stdin: Any,
        stdout: Any,
        stderr: Any,
    ):
        if not self.is_alive():
            self.start_daemon()

        


live_daemons: dict[str, DaemonTool] = {}


def invoke_llvm_daemon_tool(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO):
    pass
