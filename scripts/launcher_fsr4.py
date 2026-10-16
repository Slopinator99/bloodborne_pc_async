#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The FSR 4 asset list for bb-launcher's Graphics tab, as tab-separated lines on stdout:

    base<TAB>URL the files are downloaded from (one file per name below it)
    file<TAB>name<TAB>1 if it is already in <port folder>/fsr4_shaders, else 0

Usage: launcher_fsr4.py <port folder>

The list and the address are scripts/fsr4_assets.py, shared with the Python launcher."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fsr4_assets  # noqa: E402


def lines(port):
    folder = Path(port) / 'fsr4_shaders'
    missing = set(fsr4_assets.fsr4_missing(folder))
    return [f'base\t{fsr4_assets.FSR4_BASE}'] + [
        f'file\t{name}\t{0 if name in missing else 1}' for name in fsr4_assets.fsr4_files()]


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    for line in lines(argv[1]):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
