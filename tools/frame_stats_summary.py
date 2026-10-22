#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Summarises the "Frame stats:" lines of a game log (BB_FRAME_STATS, on by default in the
Windows launchers), to compare two runs: frame rate, hitches, pipeline compiles, async work.

    frame_stats_summary.py launcher-run.log [other.log ...]

One block per log. A window is one Frame stats line (about a second)."""
import re
import statistics
import sys
from pathlib import Path

LINE = re.compile(
    r'Frame stats: ([\d.]+) FPS, worst frame ([\d.]+) ms.*?; (\d+) shader/pipeline compiles, ([\d.]+) ms;'
    r'.*?async (\d+) enq/(\d+) done/(\d+) skipped')


def windows(path):
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        found = LINE.search(line)
        if found:
            fps, worst, compiles, compile_ms, enq, done, skipped = found.groups()
            yield (float(fps), float(worst), int(compiles), float(compile_ms), int(enq), int(done), int(skipped))


def summary(path):
    rows = list(windows(path))
    if not rows:
        return f'{path}: no Frame stats lines (is BB_FRAME_STATS on?)'
    fps = [r[0] for r in rows]
    worst = [r[1] for r in rows]
    lines = [f'{path}: {len(rows)} windows',
             f'  frame rate   median {statistics.median(fps):.1f}, 5th percentile {sorted(fps)[len(fps) // 20]:.1f} FPS',
             f'  worst frame  median {statistics.median(worst):.1f} ms, worst of all {max(worst):.1f} ms',
             f'  hitch windows (worst frame over 50 / 100 / 250 ms): '
             f'{sum(w > 50 for w in worst)} / {sum(w > 100 for w in worst)} / {sum(w > 250 for w in worst)}',
             f'  pipeline compiles on the GPU thread: {sum(r[2] for r in rows)}, '
             f'{sum(r[3] for r in rows):.0f} ms in all, in {sum(r[2] > 0 for r in rows)} windows',
             f'  async: {sum(r[4] for r in rows)} enqueued, {sum(r[5] for r in rows)} done, '
             f'{sum(r[6] for r in rows)} draws skipped']
    return '\n'.join(lines)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    print('\n\n'.join(summary(p) for p in sys.argv[1:]))
