import getopt
import os
import pathlib
import shutil
import stat
from typing import Any, Callable, Tuple

import lit.util
from lit.ShCommands import Command
from lit.ShellEnvironment import ShellEnvironment, InternalShellError, kIsWindows, updateEnv


class InprocBuiltinIO:
    """
    Holds IO streams for an inproc builtin invocation.
    These may be open files or StringIO.
    """

    stdin: Any
    stdout: Any
    stderr: Any

    def __init__(self, stdin, stdout, stderr):
        self.stdin = stdin
        self.stdout = stdout
        self.stderr = stderr


InprocBuiltinCallable = Callable[
    [Command, list[str], ShellEnvironment, InprocBuiltinIO], 
    int,
]
"""
Function representing an in-process builtin command.
Parameters:
    - `cmd`: The command itself.
    - `args`: glob-expanded list of arguments (including argv[0] as the program name).
    - `shenv`: The shell environment.
    - `io`: Holds the input and output streams for the invocation. These are file-like objects (files, StringIO)

The return value is the exit code.
"""


def executeBuiltinCd(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO) -> int:
    """executeBuiltinCd - Change the current directory."""
    if len(args) != 2:
        raise InternalShellError(cmd, "'cd' supports only one argument")
    # Update the cwd in the parent environment.
    shenv.change_dir(args[1])
    # The cd builtin always succeeds. If the directory does not exist, the
    # following Popen calls will fail instead.
    return 0


def executeBuiltinPushd(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO) -> int:
    """executeBuiltinPushd - Change the current dir and save the old."""
    if len(args) != 2:
        raise InternalShellError(cmd, "'pushd' supports only one argument")
    shenv.dirStack.append(shenv.cwd)
    shenv.change_dir(args[1])
    return 0


def executeBuiltinPopd(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO) -> int:
    """executeBuiltinPopd - Restore a previously saved working directory."""
    if len(args) != 1:
        raise InternalShellError(cmd, "'popd' does not support arguments")
    if not shenv.dirStack:
        raise InternalShellError(cmd, "popd: directory stack empty")
    shenv.cwd = shenv.dirStack.pop()
    return 0


def executeBuiltinExport(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO) -> int:
    """executeBuiltinExport - Set an environment variable."""
    if len(args) != 2:
        raise InternalShellError(cmd, "'export' supports only one argument")
    updateEnv(shenv, args)
    return 0


def executeBuiltinEcho(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO) -> int:
    """Interpret a redirected echo or @echo command"""
    opened_files = []

    if kIsWindows and False:
        # TODO(BStott) fix this
        # Reopen stdout with `newline=""` to avoid CRLF translation.
        # The versions of echo we are replacing on Windows all emit plain LF,
        # and the LLVM tests now depend on this.
        stdout = open(f.name, f.mode, encoding="utf-8", newline="")
        opened_files.append((None, None, stdout, None))

    # Implement echo flags. We only support -e and -n, and not yet in
    # combination. We have to ignore unknown flags, because `echo "-D FOO"`
    # prints the dash.
    args = args[1:]
    interpret_escapes = False
    write_newline = True
    while len(args) >= 1 and args[0] in ("-e", "-n"):
        flag = args[0]
        args = args[1:]
        if flag == "-e":
            interpret_escapes = True
        elif flag == "-n":
            write_newline = False

    def maybeUnescape(arg):
        if not interpret_escapes:
            return arg

        return arg.encode("utf-8").decode("unicode_escape")

    if args:
        for arg in args[:-1]:
            io.stdout.write(maybeUnescape(arg))
            io.stdout.write(" ")
        io.stdout.write(maybeUnescape(args[-1]))
    if write_newline:
        io.stdout.write("\n")

    for (name, mode, f, path) in opened_files:
        f.close()

    return 0


def executeBuiltinMkdir(cmd: Command, args: list[str], cmd_shenv: ShellEnvironment, io: InprocBuiltinIO):
    """executeBuiltinMkdir - Create new directories."""
    try:
        opts, args = getopt.gnu_getopt(args[1:], "p")
    except getopt.GetoptError as err:
        raise InternalShellError(cmd, "Unsupported: 'mkdir':  %s" % str(err))

    parent = False
    for o, a in opts:
        if o == "-p":
            parent = True
        else:
            assert False, "unhandled option"

    if len(args) == 0:
        raise InternalShellError(cmd, "Error: 'mkdir' is missing an operand")

    exitCode = 0
    for dir in args:
        dir = pathlib.Path(dir)
        cwd = pathlib.Path(cmd_shenv.cwd)
        if not dir.is_absolute():
            dir = lit.util.abs_path_preserve_drive(cwd / dir)
        if parent:
            dir.mkdir(parents=True, exist_ok=True)
        else:
            try:
                dir.mkdir(exist_ok=True)
            except OSError as err:
                io.stderr.write("Error: 'mkdir' command failed, %s\n" % str(err))
                exitCode = 1
    return exitCode


def executeBuiltinRm(cmd: Command, args: list[str], cmd_shenv: ShellEnvironment, io: InprocBuiltinIO):
    """executeBuiltinRm - Removes (deletes) files or directories."""
    try:
        opts, args = getopt.gnu_getopt(args[1:], "frR", ["--recursive"])
    except getopt.GetoptError as err:
        raise InternalShellError(cmd, "Unsupported: 'rm':  %s" % str(err))

    force = False
    recursive = False
    for o, a in opts:
        if o == "-f":
            force = True
        elif o in ("-r", "-R", "--recursive"):
            recursive = True
        else:
            assert False, "unhandled option"

    if len(args) == 0:
        raise InternalShellError(cmd, "Error: 'rm' is missing an operand")

    def on_rm_error(func, path, exc_info):
        # path contains the path of the file that couldn't be removed
        # let's just assume that it's read-only and remove it.
        os.chmod(path, stat.S_IMODE(os.stat(path).st_mode) | stat.S_IWRITE)
        os.remove(path)

    exitCode = 0
    for path in args:
        cwd = cmd_shenv.cwd
        if not os.path.isabs(path):
            path = lit.util.abs_path_preserve_drive(os.path.join(cwd, path))
        if force and not os.path.exists(path):
            continue
        try:
            if os.path.islink(path):
                os.remove(path)
            elif os.path.isdir(path):
                if not recursive:
                    io.stderr.write("Error: %s is a directory\n" % path)
                    exitCode = 1
                if kIsWindows:
                    # NOTE: use ctypes to access `SHFileOperationsW` on Windows to
                    # use the NT style path to get access to long file paths which
                    # cannot be removed otherwise.
                    from ctypes.wintypes import BOOL, HWND, LPCWSTR, UINT, WORD
                    from ctypes import addressof, byref, c_void_p, create_unicode_buffer
                    from ctypes import Structure
                    from ctypes import windll, WinError, POINTER

                    class SHFILEOPSTRUCTW(Structure):
                        _fields_ = [
                            ("hWnd", HWND),
                            ("wFunc", UINT),
                            ("pFrom", LPCWSTR),
                            ("pTo", LPCWSTR),
                            ("fFlags", WORD),
                            ("fAnyOperationsAborted", BOOL),
                            ("hNameMappings", c_void_p),
                            ("lpszProgressTitle", LPCWSTR),
                        ]

                    FO_MOVE, FO_COPY, FO_DELETE, FO_RENAME = range(1, 5)

                    FOF_SILENT = 4
                    FOF_NOCONFIRMATION = 16
                    FOF_NOCONFIRMMKDIR = 512
                    FOF_NOERRORUI = 1024

                    FOF_NO_UI = (
                        FOF_SILENT
                        | FOF_NOCONFIRMATION
                        | FOF_NOERRORUI
                        | FOF_NOCONFIRMMKDIR
                    )

                    SHFileOperationW = windll.shell32.SHFileOperationW
                    SHFileOperationW.argtypes = [POINTER(SHFILEOPSTRUCTW)]

                    path = os.path.abspath(path)

                    pFrom = create_unicode_buffer(path, len(path) + 2)
                    pFrom[len(path)] = pFrom[len(path) + 1] = "\0"
                    operation = SHFILEOPSTRUCTW(
                        wFunc=UINT(FO_DELETE),
                        pFrom=LPCWSTR(addressof(pFrom)),
                        fFlags=FOF_NO_UI,
                    )
                    result = SHFileOperationW(byref(operation))
                    if result:
                        raise WinError(result)
                else:
                    shutil.rmtree(path, onerror=on_rm_error if force else None)
            else:
                if force and not os.access(path, os.W_OK):
                    os.chmod(path, stat.S_IMODE(os.stat(path).st_mode) | stat.S_IWRITE)
                os.remove(path)
        except OSError as err:
            io.stderr.write("Error: 'rm' command failed, %s" % str(err))
            exitCode = 1
    return exitCode


def executeBuiltinUmask(cmd: Command, args: list[str], shenv: ShellEnvironment, io: InprocBuiltinIO):
    """executeBuiltinUmask - Change the current umask."""
    if os.name != "posix":
        raise InternalShellError(cmd, "'umask' not supported on this system")
    if len(args) != 2:
        raise InternalShellError(cmd, "'umask' supports only one argument")
    try:
        # Update the umask in the parent environment.
        shenv.umask = int(args[1], 8)
    except ValueError as err:
        raise InternalShellError(cmd, "Error: 'umask': %s" % str(err))
    return 0


def executeBuiltinUlimit(cmd: Command, args: list[str], shenv, io: InprocBuiltinIO):
    """executeBuiltinUlimit - Change the current limits."""
    try:
        # Try importing the resource module (available on POSIX systems) and
        # emit an error where it does not exist (e.g., Windows).
        import resource
    except ImportError:
        raise InternalShellError(cmd, "'ulimit' not supported on this system")
    if len(args) != 3:
        raise InternalShellError(cmd, "'ulimit' requires two arguments")
    try:
        if args[2] == "unlimited":
            new_limit = resource.RLIM_INFINITY
        else:
            new_limit = int(args[2])
    except ValueError as err:
        raise InternalShellError(cmd, "Error: 'ulimit': %s" % str(err))
    if args[1] == "-v":
        if new_limit != resource.RLIM_INFINITY:
            new_limit = new_limit * 1024
        shenv.ulimit["RLIMIT_AS"] = new_limit
    elif args[1] == "-n":
        shenv.ulimit["RLIMIT_NOFILE"] = new_limit
    elif args[1] == "-s":
        if new_limit != resource.RLIM_INFINITY:
            new_limit = new_limit * 1024
        shenv.ulimit["RLIMIT_STACK"] = new_limit
    elif args[1] == "-f":
        shenv.ulimit["RLIMIT_FSIZE"] = new_limit
    else:
        raise InternalShellError(
            cmd, "'ulimit' does not support option: %s" % args[1]
        )
    return 0


def executeBuiltinColon(cmd: Command, args: list[str], cmd_shenv: ShellEnvironment, io: InprocBuiltinIO):
    """executeBuiltinColon - Discard arguments and exit with status 0."""
    return 0


def getDefaultInprocBuiltins() -> dict[
    str,
    Tuple[InprocBuiltinCallable, bool],
]:
    """
    getDefaultInprocBuiltins - Returns the map of command names to Lit's
    in-process built-in implementations.
    The entries are a pair of the callable for the builtin and a bool
    that determines whether this inproc builtin may fall back to a
    command of the same name in cases where in-proc builtins cannot be
    used (e.g. as the argument to not --crash).
    """

    return {
        "@echo": (executeBuiltinEcho, False),
        "cd": (executeBuiltinCd, False),
        "export": (executeBuiltinExport, False),
        "echo": (executeBuiltinEcho, False),
        "mkdir": (executeBuiltinMkdir, False),
        "popd": (executeBuiltinPopd, False),
        "pushd": (executeBuiltinPushd, False),
        "rm": (executeBuiltinRm, False),
        "ulimit": (executeBuiltinUlimit, False),
        "umask": (executeBuiltinUmask, False),
        ":": (executeBuiltinColon, False),
    }

