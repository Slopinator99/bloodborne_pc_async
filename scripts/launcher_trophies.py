#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The trophies of bb-launcher's Trophies tab: one tab-separated line per trophy on stdout,

    trophy<TAB>id<TAB>group<TAB>grade<TAB>name<TAB>description<TAB>unlock time (unix seconds, 0 = locked)

Usage: launcher_trophies.py <saves folder> [game output log ...]

The unlocks are read like the Python launcher reads them: trophies.log in the saves folder (written
by the runtime), last_run.log there, and any game output log given ("Runtime: trophy N unlocked"
lines). The platinum is awarded when every base-game trophy is unlocked. The list and the rules
are launcher/bbport_trophies.py, shared with the Python launcher."""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bbport_trophies as trophies  # noqa: E402


def field(text):
    return ' '.join(str(text).split())


def lines(saves, outputs=()):
    tracker = trophies.Tracker(saves)
    tracker.poll()
    for path in outputs:
        try:
            text = Path(path).read_text(encoding='utf-8', errors='replace')
            when = int(Path(path).stat().st_mtime)
        except OSError:
            continue
        found = trophies.unlocks_in(text)
        if found:
            tracker.record(found, when)
    out = []
    for trophy_id, name, description, grade, group in trophies.TROPHIES:
        when = tracker.unlocked.get(trophy_id, 0)
        out.append('\t'.join(['trophy', str(trophy_id), field(group), grade, field(name),
                              field(description), str(int(when))]))
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    for line in lines(Path(argv[1]), argv[2:]):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
