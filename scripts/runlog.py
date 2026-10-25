# SPDX-License-Identifier: GPL-2.0-or-later
"""The run log of run.py: a header that says which build ran on what, and the game's output with
the time since the start on every line (a log without times cannot tell a freeze from a quit).

    header_lines(env, args, port_dir)   the lines printed before the game starts
    stamp_stream(lines, out, clock)     copies lines to out with "[  12.345] " in front
    attach(path, append)                makes `path` the output of this process and its children
    relay(pipe, out)                    stamps what a child process writes to a pipe
"""
import io
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import threading
import time

SECRET = re.compile(r'PASSWORD|TOKEN|SECRET|NPID|API_?KEY', re.I)
# Variables worth reading in a bug report: the port's own and the few the loaders look at.
SHOWN = re.compile(r'^(BB_|PYTHON|VK_|DXVK_|SDL_)')


def build_info(port_dir):
    """What the build says about itself: build-info.txt of a package (made by package.sh), else
    the git commit of a source tree, else ''."""
    port_dir = Path(port_dir)
    try:
        text = (port_dir / 'build-info.txt').read_text(encoding='utf-8').strip()
        if text:
            return ' '.join(text.split())
    except OSError:
        pass
    if (port_dir / '.git').exists():
        try:
            commit = subprocess.run(['git', '-C', str(port_dir), 'rev-parse', '--short', 'HEAD'],
                                    capture_output=True, text=True, timeout=5).stdout.strip()
            branch = subprocess.run(['git', '-C', str(port_dir), 'rev-parse', '--abbrev-ref', 'HEAD'],
                                    capture_output=True, text=True, timeout=5).stdout.strip()
            if commit:
                return f'source tree {branch} {commit}'
        except (OSError, subprocess.SubprocessError):
            pass
    return ''


def visible_environment(env):
    """NAME=VALUE for the variables a bug report needs, secrets hidden."""
    lines = []
    for name in sorted(env):
        if not SHOWN.match(name):
            continue
        value = '<hidden>' if SECRET.search(name) else env[name]
        lines.append(f'  {name}={value}')
    return lines


def header_lines(env, args, port_dir, now=None):
    now = time.localtime(now)
    stamp = time.strftime('%Y-%m-%d %H:%M:%S', now)
    zone = time.strftime('%z', now)
    lines = [
        f'== bbport run {stamp} {zone} ==',
        f'Build: {env.get("BB_PORT_VERSION", "?")} {build_info(port_dir)}'.rstrip(),
        f'System: {platform.platform()}, {os.cpu_count()} threads, '
        f'{"frozen runtime" if getattr(sys, "frozen", False) else "Python " + platform.python_version()}',
        f'Folder: {port_dir}',
    ]
    if args:
        lines.append('Arguments: ' + ' '.join(args))
    lines.append('Environment:')
    lines += visible_environment(env)
    lines.append('== game output (time since the start in front of each line) ==')
    return lines


def stamp_stream(lines, out, clock=time.monotonic):
    """Writes each of `lines` (str, without newline) to `out` as "[  12.345] line", flushed: a
    log must survive the game ending without a word."""
    start = clock()
    for line in lines:
        out.write(f'[{clock() - start:8.3f}] {line}\n')
        out.flush()


KEEP_LOGS = 10


def rotate(path, keep=KEEP_LOGS):
    """Moves the log of the previous run into <folder>/logs/run-<its time>.log and deletes the
    oldest beyond `keep`: a new run never destroys the log of the one before it."""
    path = Path(path)
    logs = path.parent / 'logs'
    try:
        if path.is_file() and path.stat().st_size:
            logs.mkdir(parents=True, exist_ok=True)
            stamp = time.strftime('%Y%m%d-%H%M%S', time.localtime(path.stat().st_mtime))
            target = logs / f'run-{stamp}.log'
            number = 1
            while target.exists():
                number += 1
                target = logs / f'run-{stamp}-{number}.log'
            shutil.move(str(path), str(target))
        for old in sorted(logs.glob('run-*.log'), reverse=True)[keep:]:
            old.unlink()
    except OSError:
        pass  # a log that cannot be moved is overwritten: starting the game matters more


def attach(path, append=False):
    """Makes `path` this process's standard output and error, and its children's: the log is
    then written by run.py, whatever happens to the launcher that started it. A new run first
    moves the previous log away (rotate); a restart (append) continues the same file."""
    path = Path(path)
    if not append:
        rotate(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    log = open(path, 'ab' if append else 'wb', buffering=0)
    if os.name == 'nt':
        import ctypes
        import msvcrt
        handle = msvcrt.get_osfhandle(log.fileno())
        os.set_handle_inheritable(handle, True)
        for which in (-11, -12):  # STD_OUTPUT_HANDLE, STD_ERROR_HANDLE: what children inherit
            ctypes.windll.kernel32.SetStdHandle(which, handle)
    os.dup2(log.fileno(), 1)
    os.dup2(log.fileno(), 2)
    for name, fd in (('stdout', 1), ('stderr', 2)):
        setattr(sys, name, io.TextIOWrapper(io.FileIO(fd, 'w', closefd=False), encoding='utf-8',
                                            errors='replace', write_through=True))
    return log


def relay(pipe, out=None, clock=time.monotonic):
    """Starts a thread that stamps the lines read from `pipe` (a child's stdout, bytes) and
    writes them to `out` (default: sys.stdout). Returns the thread; join it after the child ends."""
    def decoded():
        for raw in iter(pipe.readline, b''):
            yield raw.decode('utf-8', errors='replace').rstrip('\r\n')

    def work():
        stamp_stream(decoded(), out or sys.stdout, clock)

    thread = threading.Thread(target=work, daemon=True)
    thread.start()
    return thread
