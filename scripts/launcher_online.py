#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Online co-op for bb-launcher: the connection test, the environment the game gets, and the
files of The Hunter's Dream sign-in (bbbridge). The rules are the Python launcher's own
(launcher/bbport_launcher_win.py: server_check, game_environment, bridge_*), so the native
launcher keeps no copy of them.

    launcher_online.py check SERVER WEBAPI COMMUNITY POOL SAVES ONLINE(0|1)
        a line about each address: reachable, or why not; POOL is shadnet or huntersdream
    launcher_online.py env
        NAME=VALUE lines, the BB_ variables of the online settings in settings.json (empty when
        "Play online" is off); the SuperMedo Server pool of the old launcher is read as shadNet
    launcher_online.py bridge SAVES
        cli<TAB>path of bbbridge-cli.exe (empty when missing), folder<TAB>, config<TAB> (made if
        absent), then account<TAB>id for each Hunter's Dream id signed in with Discord"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
argv0, sys.argv = sys.argv, ['bbport_launcher_win']   # the launcher module reads sys.argv at import
import bbport_launcher_win as win  # noqa: E402
sys.argv = argv0

ONLINE_PREFIXES = ('BB_ONLINE', 'BB_SHADNET', 'BB_UPNP', 'BB_COMMUNITY', 'BB_BRIDGE', 'BB_POOL')


def check(server, webapi, community, pool, saves, online):
    values = {'online_server': server, 'online_webapi': webapi, 'online_community': community}
    address = server.strip() or win.APP_DEFAULTS['online_server']
    api, game = win.webapi_setting(values), win.community_address(community)
    if pool == 'huntersdream':
        text = win.server_check_hunters_dream()
        signed = win.bridge_accounts(saves)
        text += '\n' + (f'Signed in as {", ".join(signed)}' if signed else 'Not signed in: use "Sign in with Discord".')
        if not win.bridge_cli():
            text += '\nbbbridge-cli.exe is missing from this installation.'
    else:
        text = win.server_check(address, api, game)
        if webapi.strip() and webapi.strip() != api:
            text += '\nThe WebAPI box holds the game server address; it is ignored.'
    if not online:
        text += '\n"Play online" is off: the game starts offline.'
    return text


def environment():
    settings = {**win.APP_DEFAULTS, **win.load_json(win.CONFIG_FILE, {})}
    if not settings.get('online'):
        return []
    if settings.get('online_pool') not in ('shadnet', 'huntersdream'):
        settings['online_pool'] = 'shadnet'
    env = win.game_environment(settings)
    return [f'{name}={value}' for name, value in sorted(env.items())
            if name.startswith(ONLINE_PREFIXES) and value != os.environ.get(name)]


def bridge(saves):
    cli = win.bridge_cli()
    folder, config = win.bridge_config(saves)
    out = [f'cli\t{cli or ""}', f'folder\t{folder}', f'config\t{config}']
    out += [f'account\t{account}' for account in win.bridge_accounts(saves)]
    return out


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    command, args = (argv[1], argv[2:]) if len(argv) > 1 else ('', [])
    if command == 'check' and len(args) == 6:
        print(check(args[0], args[1], args[2], args[3], args[4], args[5] == '1'))
        return 0
    if command == 'env' and not args:
        print('\n'.join(environment()))
        return 0
    if command == 'bridge' and len(args) == 1:
        try:
            print('\n'.join(bridge(args[0])))
        except OSError as error:
            print(error, file=sys.stderr)
            return 1
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
