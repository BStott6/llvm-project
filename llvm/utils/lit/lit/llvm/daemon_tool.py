import subprocess
from threading import Thread
from typing import Any, Optional

from lit.InprocBuiltins import InprocBuiltinIO
from lit.ShCommands import Command
from lit.ShellEnvironment import ShellEnvironment, InternalShellError, kIsWindows, updateEnv


try:
    from queue import Queue, Empty
except ImportError:
    from Queue import Queue, Empty


def async_enqueue_stderr(daemon_tool: "DaemonTool"):
    while daemon_tool.is_alive():
        daemon_tool.stderr_queue.put(
            daemon_tool.daemon_proc.stderr.readline()
        )


class DaemonTool:
    executable_path: str
    daemon_proc: Optional[subprocess.Popen]
    stderr_queue: Queue
    stderr_enqueueing_thread: Optional[Thread]

    def __init__(self, executable_path: str):
        self.executable_path = executable_path
        self.daemon_proc = None 
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

        # Close the old stderr enqueueing thread.
        if self.stderr_enqueueing_thread:
            self.stderr_enqueueing_thread.join()

        self.daemon_proc = subprocess.Popen(
            self.executable_path,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        self.stderr_enqueueing_thread = Thread(target=async_enqueue_stderr, args=[self])
        self.stderr_enqueueing_thread.daemon = True
        self.stderr_enqueueing_thread.start()

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
