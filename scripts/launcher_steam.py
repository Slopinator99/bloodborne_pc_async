#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Adds the game to Steam as a non-Steam shortcut, for bb-launcher's "Add to Steam" button.

    launcher_steam.py status [--steam-dir DIR]
    launcher_steam.py add NAME EXE START_DIR ICON [--options TEXT] [--steam-dir DIR] [--close-steam]

status prints  steam<TAB>folder (empty when Steam is not installed)  accounts<TAB>count of the
accounts that signed in on this PC  running<TAB>1|0.

add writes the entry into the shortcuts.vdf of every account (a copy of each old file is kept as
shortcuts.vdf.bak) and prints added or updated. Steam rewrites that file as it exits, so with
--close-steam a running Steam is asked to shut down first, waited for, and started again
afterwards. Exit codes: 0 done, 2 usage, 3 no Steam or no account, 4 Steam did not close,
1 another error.

The format and the rules are launcher/bbport_steam.py, shared with the Python launcher."""
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
import bbport_steam  # noqa: E402

NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def option(args, name, default=None):
    """The value after name in args (removed from it), or default."""
    if name in args:
        index = args.index(name)
        value = args[index + 1] if index + 1 < len(args) else default
        del args[index:index + 2]
        return value
    return default


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    args = list(argv[1:])
    steam_dir = option(args, '--steam-dir')
    options = option(args, '--options', '')
    close_steam = '--close-steam' in args
    if close_steam:
        args.remove('--close-steam')
    steam = Path(steam_dir) if steam_dir else bbport_steam.steam_folder()
    configs = bbport_steam.user_configs(steam) if steam else []
    if args[:1] == ['status'] and len(args) == 1:
        print(f'steam\t{steam or ""}')
        print(f'accounts\t{len(configs)}')
        print(f'running\t{1 if bbport_steam.steam_running() else 0}')
        return 0
    if args[:1] == ['add'] and len(args) == 5:
        name, exe, start_dir, icon = args[1:]
        if not configs:
            print('Steam was not found on this PC (or nobody has signed in to it yet).', file=sys.stderr)
            return 3
        restart = False
        if close_steam and bbport_steam.steam_running():
            subprocess.Popen([str(steam / 'steam.exe'), '-shutdown'], creationflags=NO_WINDOW)
            for _attempt in range(60):
                time.sleep(0.5)
                if not bbport_steam.steam_running():
                    break
            else:
                print('Steam did not close.', file=sys.stderr)
                return 4
            time.sleep(1)  # Steam writes its files as it exits
            restart = True
        try:
            results = [bbport_steam.add_shortcut(config / 'shortcuts.vdf', name, exe, start_dir, icon, options)
                       for config in configs]
        except (OSError, ValueError) as error:
            print(f'Could not add the game to Steam: {error}', file=sys.stderr)
            return 1
        if restart:
            os.startfile('steam://open/games')
        print('updated' if all(r == 'updated' for r in results) else 'added')
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
