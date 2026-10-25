# Logs, hangs and crash reports

A log has to answer three questions after the fact: which build ran on what, when did it stop,
and where were the threads. This is what the port writes, and what to send with a bug report.

## For players

- **Log tab → Save diagnostics** makes one zip on the Desktop and shows it in Explorer. Send
  that file. It holds the latest log and the ten before it, any crash or hang dumps, your
  settings (passwords, tokens and co-op invite codes are removed) and the graphics card.
- **Log tab → Logs folder** opens `logs\` next to `bb-launcher.exe`.
- A new run never overwrites the log of the one before it: the previous `launcher-run.log` is
  moved to `logs\run-<date>-<time>.log` (ten are kept). Restarting after a crash is safe.
- Tell us what you saw (a black screen, a freeze, a message) and where in the game.

## What is in a log

- **Header:** the build (`build-info.txt` of a package, else the git commit), the system, the
  folder, the arguments and every `BB_` variable the game was started with. Secrets are shown as
  `<hidden>`.
- **Times:** every line of the game's output starts with the seconds since the start,
  `[  21.375]`. Lines before the game starts (preparation) have none.
- **Frame stats** (on by default in the launchers): every five seconds the frame rate, the worst
  frame, the pipeline compiles on the GPU thread and, with async pipelines, why they stayed there.
- **The end:** `run.py: bb-probe exited with code N at <time>`.
  0 is a quit (the window was closed, or the game's menu), 3 is a crash (a `Fault:` line before
  it), another code is an assertion or a failed start.
- **Window closed:** `Window closed by the player; N frames shown, the last one started M ms
  ago`. A few milliseconds is a normal quit; seconds is a game that had stopped, closed by the
  player.

## Hangs and crashes

- **Hang:** if no frame starts for 10 seconds (`BB_HANG_SECONDS`), the log gets
  `HANG: no frame for N s`, a dump `logs\hang-<time>.dmp` is written once, and
  `HANG over: frames resumed after about N s` says when it ended. Closing a frozen game writes
  `logs\closed-frozen-<time>.dmp` as well.
- **Crash:** a fatal fault prints `Fault: …` with the registers and a short stack, writes
  `logs\crash-<time>.dmp` and ends the game with exit code 3.
- The dumps are minidumps (about 2 MB: every thread's stack, the modules and, for a crash, the
  exception). Open them with WinDbg, Visual Studio, or `lldb -c file.dmp` and `thread list`,
  `bt all`. At most three are written per run.

## Settings and switches (developers)

| Variable | Meaning |
|---|---|
| `BB_RUN_LOG=<file>` | `run.py` writes the run's log to this file (the launchers set it). Without it the output goes where it was started. |
| `BB_LOG_DIR=<folder>` | Where the dumps go (set beside `BB_RUN_LOG`: `logs\`). |
| `BB_HANG_SECONDS=N` | The hang limit, default 10. |
| `BB_TEST_HANG_FRAME=N` | Test: stalls the GPU thread for 15 s at frame N (the HANG line, the dump, HANG over). |
| `BB_TEST_CRASH_FRAME=N` | Test: faults at frame N (the Fault lines, the dump, exit code 3). |
| `BB_FRAME_DUMP_TRIGGER`, `BB_DUMP_TRIGGER`, `BB_DUMP_DIR` | Picture dumps of the displayed frame and of the upscaler's inputs (see `docs/upscaler.md`). |

Code: `scripts/runlog.py` (header, times, rotation), `src/probe.c` (`bb_write_minidump`, the fault
handler), `gpu/shim/bbgpu.cpp` (the hang watchdog, the window-close line),
`scripts/launcher_diag.py` (the zip). Tests: `tests/test_runlog.py`, `tests/test_launcher_diag.py`.
