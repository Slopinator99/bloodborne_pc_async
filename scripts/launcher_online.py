#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Hunter's Dream for bb-launcher: the connection test, the Discord sign-in and the
environment the game gets. The rules are the launcher module's own
(launcher/bbport_launcher_win.py: server_check_hunters_dream, thd_sign_in, game_environment),
so the native launcher keeps no copy of them.

    launcher_online.py check COMMUNITY
        a line about each address of the game server: reachable, or why not
    launcher_online.py signin COMMUNITY
        the Discord sign-in by device code, one tab-separated line at a time:
        code<TAB>page<TAB>code, then ok<TAB>online id<TAB>token or error<TAB>reason
    launcher_online.py env
        NAME=VALUE lines, the BB_ variables of the play mode in settings.json: The Hunter's
        Dream's, empty for the other modes (co-op has launcher_coop.py env)"""
import os
from pathlib import Path
import queue
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
argv0, sys.argv = sys.argv, ['bbport_launcher_win']   # the launcher module reads sys.argv at import
import bbport_launcher_win as win  # noqa: E402
sys.argv = argv0

ONLINE_PREFIXES = ('BB_ONLINE', 'BB_SHADNET', 'BB_UPNP', 'BB_COMMUNITY', 'BB_NET_BACKEND', 'BB_THD',
                   'BB_CLIENT_VERSION')


def check(community):
    return win.server_check_hunters_dream(win.community_address(community))


def settings():
    """settings.json over the launcher's defaults; the play mode decides whether the game is online."""
    values = {**win.APP_DEFAULTS, **win.load_json(win.CONFIG_FILE, {})}
    mode = win.play_mode(values)
    values['play_mode'] = mode
    values['online'] = mode == 'huntersdream'
    if mode == 'huntersdream':
        values['online_pool'] = 'huntersdream'
    return values


def environment():
    values = settings()
    if values['play_mode'] != 'huntersdream':
        return []
    env = win.game_environment(values)
    return [f'{name}={value}' for name, value in sorted(env.items())
            if name.startswith(ONLINE_PREFIXES) and value != os.environ.get(name)]


def sign_in(community):
    """Yields the protocol lines of the sign-in."""
    # thd_sign_in blocks while it polls; the code line has to leave before that.
    out = queue.Queue()
    result = {}

    def work():
        try:
            result['ok'] = win.thd_sign_in(community, lambda page, code: out.put(f'code\t{page}\t{code}'))
        except ValueError as error:
            result['error'] = str(error)
        out.put(None)

    threading.Thread(target=work, daemon=True).start()
    while (line := out.get()) is not None:
        yield line
    if 'ok' in result:
        yield 'ok\t{}\t{}'.format(*result['ok'])
    else:
        yield 'error\t' + result.get('error', 'the sign-in failed').replace('\n', ' ')


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    command, args = (argv[1], argv[2:]) if len(argv) > 1 else ('', [])
    if command == 'check' and len(args) == 1:
        print(check(args[0]))
        return 0
    if command == 'signin' and len(args) == 1:
        for line in sign_in(args[0]):
            print(line, flush=True)
        return 0
    if command == 'env' and not args:
        print('\n'.join(environment()))
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
