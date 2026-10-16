#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The mod and patch lists of bb-launcher's Mods tab, as tab-separated lines on stdout:

    mod<TAB>name
    patch<TAB>key<TAB>author<TAB>1 if the patch file enables it by default, else 0

Usage: launcher_lists.py <mods folder> <patches folder> [game version, default 01.09]

The discovery rules live in mods.discover and patches.external_patches, shared with the Python
launcher and run.py, so the native launcher never has its own copy of them."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mods import discover  # noqa: E402
from patches import external_patches  # noqa: E402


def field(text):
    """One tab-separated field: no tabs or line breaks."""
    return ' '.join(str(text or '').split())


def lines(mods_dir, patches_dir, version='01.09'):
    out = [f'mod\t{field(name)}' for name in discover(mods_dir)]
    for key, _path, meta in external_patches(patches_dir, version):
        enabled = '1' if meta.get('isEnabled', 'false').lower() == 'true' else '0'
        out.append(f'patch\t{field(key)}\t{field(meta.get("Author"))}\t{enabled}')
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    for line in lines(Path(argv[1]), Path(argv[2]), argv[3] if len(argv) > 3 else '01.09'):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
