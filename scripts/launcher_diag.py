#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The diagnostics zip of bb-launcher's Log tab: everything a bug report needs in one file the
player can send. Passwords, tokens and invite codes are removed from the settings it holds.

    launcher_diag.py PORT_FOLDER [OUTPUT_FOLDER]
        writes bbport-diagnostics-<time>.zip (to OUTPUT_FOLDER, default the Desktop) and prints
        ok<TAB>path, or error<TAB>reason

In the zip: diagnostics.txt (build, system, graphics card, a list of what follows), the latest
run log and the ten before it (logs/), crash and hang dumps, settings.json and bbport.ini with
the secrets hidden, and the names of the mods and patches."""
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time
import zipfile

SECRET_KEY = re.compile(r'password|token|secret|npid|invite|api_?key', re.I)   # not key_interact=E: the bindings help
DUMPS_KEPT = 3


def redact_json(text):
    """settings.json with every secret value replaced; unreadable text is replaced whole."""
    try:
        data = json.loads(text)
    except ValueError:
        return '(settings.json could not be read, left out)'
    for key in list(data):
        if SECRET_KEY.search(key) and data[key] not in ('', None, False):
            data[key] = '<hidden>'
    return json.dumps(data, indent=2, ensure_ascii=False)


def redact_ini(text):
    lines = []
    for line in text.splitlines():
        key = line.split('=', 1)[0]
        lines.append(f'{key}=<hidden>' if '=' in line and SECRET_KEY.search(key) else line)
    return '\n'.join(lines) + '\n'


def graphics_cards(port):
    """What bb-gpu-capabilities.exe says about the cards (empty when it is missing)."""
    for folder in (port / 'bin', port / 'out', port):
        exe = folder / 'bb-gpu-capabilities.exe'
        if exe.is_file():
            try:
                done = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, cwd=str(folder),
                                      creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                return (done.stdout + done.stderr).strip()
            except (OSError, subprocess.SubprocessError) as error:
                return f'bb-gpu-capabilities failed: {error}'
    return ''


def newest(folder, pattern, count):
    try:
        return sorted(Path(folder).glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True)[:count]
    except OSError:
        return []


def config_dir():
    return Path(os.environ.get('APPDATA') or Path.home()) / 'bbport-launcher'


def summary(port, members):
    build = ''
    try:
        build = (port / 'build-info.txt').read_text(encoding='utf-8').strip()
    except OSError:
        pass
    lines = [
        'bbport diagnostics', time.strftime('%Y-%m-%d %H:%M:%S %z'), '',
        f'Build: {build or "?"}', f'Folder: {port}',
        f'System: {platform.platform()}, {os.cpu_count()} threads', '',
        'Graphics:', graphics_cards(port) or '(not available)', '', 'Files in this zip:',
    ]
    lines += [f'  {name}' for name in members]
    return '\n'.join(lines) + '\n'


def collect(port, out_folder):
    port = Path(port)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    target = Path(out_folder) / f'bbport-diagnostics-{stamp}.zip'
    members = []   # (name in the zip, bytes)

    def add_file(name, path):
        try:
            members.append((name, Path(path).read_bytes()))
        except OSError:
            pass

    add_file('launcher-run.log', port / 'launcher-run.log')
    for log in newest(port / 'logs', 'run-*.log', 10):
        add_file(f'logs/{log.name}', log)
    for dump in newest(port / 'logs', '*.dmp', DUMPS_KEPT):
        add_file(f'logs/{dump.name}', dump)
    settings = config_dir() / 'settings.json'
    if settings.is_file():
        members.append(('settings.json', redact_json(settings.read_text(encoding='utf-8', errors='replace')).encode('utf-8')))
    ini = port / 'bbport.ini'
    if ini.is_file():
        members.append(('bbport.ini', redact_ini(ini.read_text(encoding='utf-8', errors='replace')).encode('utf-8')))
    for name, folder in (('mods.txt', port / 'mods'), ('patches.txt', port / 'patches')):
        try:
            listing = '\n'.join(sorted(p.name for p in folder.iterdir())) + '\n'
            members.append((name, listing.encode('utf-8')))
        except OSError:
            pass
    names = ['diagnostics.txt'] + [name for name, _ in members]
    Path(out_folder).mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('diagnostics.txt', summary(port, names))
        for name, data in members:
            archive.writestr(name, data)
    return target


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    if len(argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2
    out = argv[2] if len(argv) == 3 else str(Path(os.environ.get('USERPROFILE') or Path.home()) / 'Desktop')
    try:
        print(f'ok\t{collect(argv[1], out)}', flush=True)
    except (OSError, zipfile.BadZipFile) as error:
        print(f'error\t{error}', flush=True)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
