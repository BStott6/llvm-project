import abc
from dataclasses import dataclass
from typing import Any, Union


@dataclass
class StdinFile:
    file: Any

StdinSource = Union[None, StdinFile, str, bytes]


@dataclass
class SpecialCommandInvocation:
    exitCode: int
    stdout: str
    stderr: str


class SpecialCommand(abc.ABC):
    """
    Interface for built-in commands that aren't just running an executable.
    These can take stdin from the previous command, so they can be used inside of a command chain.
    Special commands may be provided to Lit via the Python config.
    """

    @abc.abstractmethod
    def invoke(
        self,
        arguments: list[str],
        stdinSource: StdinSource,
        stderrIsStdout: bool,
    ):
        del arguments, stdinSource, stderrIsStdout
        raise NotImplementedError()
