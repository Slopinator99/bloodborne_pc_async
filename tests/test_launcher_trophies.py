"""scripts/launcher_trophies.py: the trophy lines the native launcher shows."""
from paths import ROOT
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_trophies.py'
sys.path.insert(0, str(ROOT / 'launcher'))
import bbport_trophies as trophies  # noqa: E402


def run(*args):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    rows = [line.split('\t') for line in done.stdout.splitlines()]
    return done.returncode, rows, done.stderr


class LauncherTrophiesTests(unittest.TestCase):
    def test_lists_every_trophy_locked_for_a_fresh_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            code, rows, err = run(directory)
            self.assertEqual(code, 0, err)
            self.assertEqual(len(rows), len(trophies.TROPHIES))
            self.assertTrue(all(len(row) == 7 and row[0] == 'trophy' for row in rows))
            self.assertTrue(all(row[6] == '0' for row in rows))
            first = rows[0]
            self.assertEqual((first[1], first[3], first[4]), ('0', 'platinum', 'Bloodborne'))

    def test_reads_trophies_log_and_game_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'trophies.log').write_text('21 1700000000\nbroken line\n14 1700000500\n', encoding='utf-8')
            output = root / 'launcher-run.log'
            output.write_text('Runtime: trophy 7 unlocked\nnoise\n', encoding='utf-8')
            code, rows, err = run(root, output)
            self.assertEqual(code, 0, err)
            when = {int(row[1]): int(row[6]) for row in rows}
            self.assertEqual(when[21], 1700000000)
            self.assertEqual(when[14], 1700000500)
            self.assertGreater(when[7], 0)          # seen in the output log
            self.assertEqual(when[8], 0)
            # What came from the output was written to trophies.log, as the Python launcher does.
            self.assertIn('7 ', (root / 'trophies.log').read_text(encoding='utf-8'))

    def test_platinum_when_every_base_trophy_is_unlocked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = sorted(trophies.PLATINUM_NEEDS)
            (root / 'trophies.log').write_text(''.join(f'{t} 1700000000\n' for t in base), encoding='utf-8')
            code, rows, err = run(root)
            self.assertEqual(code, 0, err)
            self.assertGreater({int(r[1]): int(r[6]) for r in rows}[trophies.PLATINUM_ID], 0)

    def test_usage_error(self):
        done = subprocess.run([sys.executable, '-I', str(SCRIPT)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
        self.assertEqual(done.returncode, 2)


if __name__ == '__main__':
    unittest.main()
