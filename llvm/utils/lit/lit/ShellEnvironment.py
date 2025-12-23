import os
import platform

import lit.util


kIsWindows = platform.system() == "Windows"

# Don't use close_fds on Windows.
kUseCloseFDs = not kIsWindows

# Use temporary files to replace /dev/null on Windows.
kAvoidDevNull = kIsWindows
kDevNull = "/dev/null"


class InternalShellError(Exception):
    def __init__(self, command, message):
        self.command = command
        self.message = message


class ShellEnvironment(object):

    """Mutable shell environment containing things like CWD and env vars.

    Environment variables are not implemented, but cwd tracking is. In addition,
    we maintain a dir stack for pushd/popd.
    """

    def __init__(self, cwd, env, umask=-1, ulimit=None):
        self.cwd = cwd
        self.env = dict(env)
        self.umask = umask
        self.dirStack = []
        self.ulimit = ulimit if ulimit else {}

    def change_dir(self, newdir):
        if os.path.isabs(newdir):
            self.cwd = newdir
        else:
            self.cwd = lit.util.abs_path_preserve_drive(os.path.join(self.cwd, newdir))


# args are from 'export' or 'env' command.
# Skips the command, and parses its arguments.
# Modifies env accordingly.
# Returns copy of args without the command or its arguments.
def updateEnv(env, args):
    arg_idx_next = len(args)
    unset_next_env_var = False
    for arg_idx, arg in enumerate(args[1:]):
        # Support for the -u flag (unsetting) for env command
        # e.g., env -u FOO -u BAR will remove both FOO and BAR
        # from the environment.
        if arg == "-u":
            unset_next_env_var = True
            continue
        # Support for the -i flag which clears the environment
        if arg == "-i":
            env.env = {}
            continue
        if unset_next_env_var:
            unset_next_env_var = False
            if arg in env.env:
                del env.env[arg]
            continue

        # Partition the string into KEY=VALUE.
        key, eq, val = arg.partition("=")
        # Stop if there was no equals.
        if eq == "":
            arg_idx_next = arg_idx + 1
            break
        env.env[key] = val
    return args[arg_idx_next:]

