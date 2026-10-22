#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Seamless co-op (BBSC) for bb-launcher. The rules are launcher/bbport_coop.py's own (downloads
checked against the release's SHA-256, firewall setup, the host's servers, invite codes,
accounts, the game's environment), so the native launcher keeps no copy of them. Every command
prints tab-separated lines the launcher shows as they arrive.

    launcher_coop.py status
        client<TAB>0|1, server<TAB>0|1 (the BBSC packages are downloaded), setup<TAB>0|1 (the
        firewall rules), hosting<TAB>0|1<TAB>address, then invite<TAB>code while hosting
    launcher_coop.py setup
        downloads what is missing and sets the firewall up (one Windows prompt); status lines,
        then ok<TAB>message or error<TAB>reason
    launcher_coop.py host
        starts the servers on this PC, makes the co-op account from settings.json and prints
        invite<TAB>code, then ok<TAB>message (the servers keep running after this exits)
    launcher_coop.py stop
        ends the servers this PC hosts
    launcher_coop.py env
        NAME=VALUE lines to start the game in co-op: hosting here, or joining the invite code in
        settings.json; makes the account and backs the saves up first. error<TAB>reason, exit 1,
        when it cannot."""
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import launcher_online  # noqa: E402  (imports the launcher module and the sys.path it needs)
win = launcher_online.win
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
import bbport_coop as coop  # noqa: E402

NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def say(kind, *values):
    print('\t'.join([kind, *values]), flush=True)


def state_file():
    return coop.coop_dir() / 'hosting.json'


def hosting():
    """(address, pids) of the servers this PC runs, or None."""
    try:
        state = json.loads(state_file().read_text(encoding='utf-8'))
        address, pids = str(state['address']), [int(p) for p in state['pids']]
    except (OSError, ValueError, KeyError, TypeError):
        return None
    alive = [p for p in pids if pid_alive(p)]
    return (address, alive) if alive else None


def pid_alive(pid):
    out = subprocess.run(['tasklist', '/FI', f'PID eq {pid}', '/FO', 'CSV', '/NH'], capture_output=True,
                         text=True, creationflags=NO_WINDOW).stdout
    return f'"{pid}"' in out


def game_exe():
    for folder in (win.PORT_DIR / 'bin', win.PORT_DIR / 'out', win.PORT_DIR):
        if (folder / 'bb-probe.exe').is_file():
            return folder / 'bb-probe.exe'
    raise coop.CoopError('bb-probe.exe is missing from this install.')


def account():
    values = launcher_online.settings()
    return str(values.get('coop_name', '')).strip(), str(values.get('coop_password', '')), values


def ensure_packages():
    for kind in ('client', 'server'):
        if not coop.installed(kind):
            coop.download(kind, progress=lambda text: say('status', text))


def ensure_setup(game):
    if not coop.setup_done(game):
        say('status', 'First time: say Yes to the Windows prompt (firewall and C++ runtime)...')
        coop.run_setup(game)


def status():
    game = None
    try:
        game = game_exe()
    except coop.CoopError:
        pass
    say('client', '1' if coop.installed('client') else '0')
    say('server', '1' if coop.installed('server') else '0')
    say('setup', '1' if game and coop.setup_done(game) else '0')
    state = hosting()
    say('hosting', '1' if state else '0', state[0] if state else '')
    if state:
        say('invite', coop.make_invite(state[0], coop.registration_key()))


def setup():
    ensure_packages()
    ensure_setup(game_exe())
    say('ok', 'Co-op is set up.')


def host():
    name, password, _values = account()
    if not name or not password:
        raise coop.CoopError('Fill in your co-op name and password first; they make your account on your server.')
    stop()
    ensure_packages()
    address, network = coop.host_address()
    if not address:
        raise coop.CoopError('No network found. Connect Tailscale, then try again.')
    ensure_setup(game_exe())
    key = coop.registration_key()
    server = coop.Host()
    server.start(address)
    pids = [p.pid for p in server.processes]
    state_file().parent.mkdir(parents=True, exist_ok=True)
    state_file().write_text(json.dumps({'address': address, 'pids': pids}), encoding='utf-8')
    how = coop.create_account(address, key, name, password)
    say('invite', coop.make_invite(address, key))
    say('ok', f'Hosting on {network} ({address}); your account is {how}. Send the invite code to your friend.')


def stop():
    state = hosting()
    if state:
        for pid in state[1]:
            subprocess.run(['taskkill', '/PID', str(pid), '/T', '/F'], capture_output=True, creationflags=NO_WINDOW)
    for kind in ('server',):
        folder = coop.package_dir(kind) / 'server'
        if folder.is_dir():
            coop.end_leftovers(folder)
    coop_bbport = coop.coop_dir() / 'shadnet-bbport'
    if coop_bbport.is_dir():
        coop.end_leftovers(coop_bbport)
    try:
        state_file().unlink()
    except OSError:
        pass


def environment():
    name, password, values = account()
    if not name or not password:
        raise coop.CoopError('Choose a co-op name and password first (Online tab).')
    state = hosting()
    if state:
        address, key = state[0], coop.registration_key()
    else:
        try:
            address, key = coop.read_invite(str(values.get('coop_invite', '')))
        except coop.CoopError as error:
            raise coop.CoopError(f'{error}. Or host a session yourself.') from None
    if not (coop.installed('client') and coop.installed('server')):
        raise coop.CoopError('Set up co-op first (Online tab: Set up co-op).')
    game = game_exe()
    if not coop.setup_done(game):
        raise coop.CoopError('Set up co-op first (Online tab: Set up co-op).')
    coop.create_account(address, key, name, password)
    coop.backup_saves(values.get('user_dir') or win.DATA_DIR / 'user')
    values['online'] = False
    env = coop.co_op_environment(win.game_environment(values), address, name, password, game)
    return [f'{n}={v}' for n, v in sorted(env.items()) if v != os.environ.get(n)]


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    command = argv[1] if len(argv) > 1 else ''
    commands = {'status': status, 'setup': setup, 'host': host, 'stop': stop}
    try:
        if command in commands and len(argv) == 2:
            commands[command]()
            return 0
        if command == 'env' and len(argv) == 2:
            print('\n'.join(environment()))
            return 0
    except (coop.CoopError, OSError, ValueError) as error:
        say('error', str(error).replace('\n', ' '))
        return 1
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
