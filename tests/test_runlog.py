"""scripts/runlog.py: the header, the timestamps, the rotation that keeps old logs, and the log
file run.py owns (its own output and its children's)."""
from paths import ROOT
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(ROOT / 'scripts'))
import runlog  # noqa: E402


class HeaderTests(unittest.TestCase):
    def lines(self, env, args=()):
        return runlog.header_lines(env, list(args), ROOT, now=1_700_000_000)

    def test_says_the_build_the_system_and_the_settings(self):
        text = '\n'.join(self.lines({'BB_PORT_VERSION': '2.1', 'BB_FPS': '60', 'BB_ASYNC_PIPELINES': '1'}, ['--x']))
        self.assertIn('== bbport run 2023-', text)
        self.assertIn('Build: 2.1', text)
        self.assertIn('System: ', text)
        self.assertIn('Arguments: --x', text)
        self.assertIn('  BB_FPS=60', text)
        self.assertIn('  BB_ASYNC_PIPELINES=1', text)

    def test_secrets_never_reach_a_log_that_gets_sent_around(self):
        text = '\n'.join(self.lines({'BB_THD_TOKEN': 'abc', 'BB_SHADNET_PASSWORD': 'pw', 'BB_SHADNET_NPID': 'me',
                                     'BB_FPS': '60', 'BB_COOP_SECRET_KEY': 'k'}))
        for secret in ('abc', 'pw', '=me', '=k'):
            self.assertNotIn(secret + '\n', text + '\n')
        self.assertIn('BB_THD_TOKEN=<hidden>', text)
        self.assertIn('BB_FPS=60', text)

    def test_only_the_relevant_variables_are_listed(self):
        text = '\n'.join(self.lines({'PATH': 'C:\\secret-looking\\path', 'USERNAME': 'someone', 'BB_FPS': '60'}))
        self.assertNotIn('PATH=', text)
        self.assertNotIn('USERNAME', text)

    def test_the_build_info_file_of_a_package_is_used(self):
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'build-info.txt').write_text('commit abc1234 2026-10-25\n', encoding='utf-8')
            self.assertEqual(runlog.build_info(folder), 'commit abc1234 2026-10-25')


class StampTests(unittest.TestCase):
    def test_each_line_carries_the_time_since_the_start(self):
        ticks = iter([100.0, 100.5, 101.25, 160.0])
        out = io.StringIO()
        runlog.stamp_stream(['one', 'two', 'three'], out, clock=lambda: next(ticks))
        self.assertEqual(out.getvalue().splitlines(),
                         ['[   0.500] one', '[   1.250] two', '[  60.000] three'])


class RotationTests(unittest.TestCase):
    def test_the_previous_log_is_moved_not_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder, 'launcher-run.log')
            log.write_text('the run that crashed\n', encoding='utf-8')
            runlog.rotate(log)
            self.assertFalse(log.exists())
            kept = list(Path(folder, 'logs').glob('run-*.log'))
            self.assertEqual(len(kept), 1)
            self.assertEqual(kept[0].read_text(encoding='utf-8'), 'the run that crashed\n')

    def test_only_the_newest_ten_are_kept(self):
        with tempfile.TemporaryDirectory() as folder:
            logs = Path(folder, 'logs')
            logs.mkdir()
            for n in range(14):
                (logs / f'run-20260101-0000{n:02d}.log').write_text(str(n), encoding='utf-8')
            runlog.rotate(Path(folder, 'launcher-run.log'))
            names = sorted(p.name for p in logs.glob('run-*.log'))
            self.assertEqual(len(names), 10)
            self.assertEqual(names[0], 'run-20260101-000004.log')   # the four oldest went

    def test_an_empty_or_missing_log_leaves_nothing_behind(self):
        with tempfile.TemporaryDirectory() as folder:
            runlog.rotate(Path(folder, 'launcher-run.log'))
            self.assertEqual(list(Path(folder).glob('**/run-*.log')), [])


class AttachTests(unittest.TestCase):
    """run.py's way: its own prints and the game's (relayed) go to the file, which a restart continues."""

    def run_attach(self, folder, append):
        code = (
            'import subprocess, sys\n'
            f'sys.path.insert(0, {str(ROOT / "scripts")!r})\n'
            'import runlog\n'
            f'runlog.attach({str(Path(folder, "launcher-run.log"))!r}, append={append})\n'
            'print("from run.py", flush=True)\n'
            'game =subprocess.Popen([sys.executable, "-u", "-c", "print(\'game line 1\'); print(\'game line 2\')"],\n'
            '                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)\n'
            'thread = runlog.relay(game.stdout)\n'
            'game.wait(); thread.join(5)\n'
            'print("run.py ends", file=sys.stderr, flush=True)\n')
        done = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True, timeout=60)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(done.stdout, '')     # nothing went to the original output

    def test_its_own_output_and_the_games_land_in_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            self.run_attach(folder, False)
            text = Path(folder, 'launcher-run.log').read_text(encoding='utf-8')
            for expected in ('from run.py', '] game line 1', '] game line 2', 'run.py ends'):
                self.assertIn(expected, text)
            self.assertRegex(text, r'\[\s*\d+\.\d{3}\] game line 1')

    def test_a_second_run_keeps_the_first_in_logs_and_a_restart_appends(self):
        with tempfile.TemporaryDirectory() as folder:
            self.run_attach(folder, False)
            time.sleep(1.1)   # the rotated name carries the time
            self.run_attach(folder, False)
            self.assertEqual(len(list(Path(folder, 'logs').glob('run-*.log'))), 1)
            first = Path(folder, 'launcher-run.log').read_text(encoding='utf-8')
            self.run_attach(folder, True)      # an in-game restart
            after = Path(folder, 'launcher-run.log').read_text(encoding='utf-8')
            self.assertTrue(after.startswith(first))
            self.assertEqual(after.count('game line 1'), 2)
            self.assertEqual(len(list(Path(folder, 'logs').glob('run-*.log'))), 1)


class ScriptOutputTests(unittest.TestCase):
    """run.py's run_script: a script's output is written by run.py itself, line by line."""

    def test_a_scripts_output_reaches_the_log_and_its_exit_code_is_kept(self):
        import contextlib
        import importlib.util
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'scripts').mkdir()
            (Path(folder) / 'scripts' / 'speak.py').write_text(
                'import sys\n'
                'print("first line", flush=True)\n'
                'print("to stderr", file=sys.stderr, flush=True)\n'
                'sys.exit(int(sys.argv[1]))\n', encoding='utf-8')
            spec = importlib.util.spec_from_file_location('run_under_test', ROOT / 'run.py')
            run = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(run)
            run.PORT = Path(folder)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(run.run_script('speak.py', '0'), '')
            self.assertIn('first line', out.getvalue())
            self.assertIn('to stderr', out.getvalue())          # stderr joins it
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
                self.assertIsNone(run.run_script('speak.py', '3', optional=True))
            self.assertIn('failed (exit code 3)', out.getvalue())


if __name__ == '__main__':
    unittest.main()
