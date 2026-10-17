#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The entry point of bb-runtime.exe, the frozen Python runtime of the Windows package.

bb-runtime.exe is the engine behind the native launcher (bb-launcher.exe): it runs the game
(--run, --play), the preparation and helper scripts (--script) and the updater
(--install-update). It is built from this file, not from bbport_launcher_win.py directly, for two
reasons:

* PyInstaller does not let a program import its own entry script by name, and the helper scripts
  (scripts/launcher_*.py) use that module's rules (server_check, game_environment, ...), so the
  launcher module has to be an ordinary importable module in the frozen runtime.
* Started with no arguments, the old Tk launcher window is no longer shown when bb-launcher.exe is
  next to it: double-clicking bb-runtime.exe opens the native launcher instead. The old window is
  still there in a source tree (python launcher/bbport_launcher_win.py).
"""
import subprocess
import sys
from pathlib import Path

import bbport_launcher_win as launcher


def native_launcher(args, frozen, here):
    """The bb-launcher.exe to start instead of the old window, or None."""
    if args or not frozen:
        return None
    exe = Path(here) / 'bb-launcher.exe'
    return exe if exe.is_file() else None


def main():
    exe = native_launcher(sys.argv[1:], getattr(sys, 'frozen', False), Path(sys.executable).resolve().parent)
    if exe:
        subprocess.Popen([str(exe)], cwd=str(exe.parent), close_fds=True)
        return 0
    return launcher.main()


if __name__ == '__main__':
    sys.exit(main())
