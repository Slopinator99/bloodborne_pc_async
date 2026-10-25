"""scripts/launcher_diag.py: the diagnostics zip holds what a bug report needs and no secrets."""
from paths import ROOT
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

SCRIPT = ROOT / 'scripts' / 'launcher_diag.py'


def run(port, out, appdata):
    env = dict(os.environ, APPDATA=str(appdata))
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), str(port), str(out)], capture_output=True, text=True,
                          encoding='utf-8', env=env, timeout=120)
    return done.returncode, done.stdout.strip(), done.stderr


class DiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.port, self.out, self.appdata = root / 'port', root / 'out', root / 'appdata'
        (self.port / 'logs').mkdir(parents=True)
        (self.appdata / 'bbport-launcher').mkdir(parents=True)
        (self.port / 'launcher-run.log').write_text('the latest run\n', encoding='utf-8')
        for n in range(12):
            (self.port / 'logs' / f'run-20260101-0000{n:02d}.log').write_text(f'old {n}\n', encoding='utf-8')
        for n in range(5):
            (self.port / 'logs' / f'hang-20260101-0000{n:02d}.dmp').write_bytes(b'MDMP' * 4)
        (self.port / 'mods' / 'Some Mod').mkdir(parents=True)
        (self.port / 'bbport.ini').write_text('upscaler=taa\nsome_token=abc\nkey_interact=E\n', encoding='utf-8')
        (self.appdata / 'bbport-launcher' / 'settings.json').write_text(json.dumps({
            'game_dir': 'D:\\Games', 'thd_token': 'secret-token', 'coop_password': 'hunter2', 'coop_invite': 'BBCOOP-xyz',
            'online_npid': 'me', 'fps_mode': '60', 'coop_name': ''}), encoding='utf-8')

    def make(self):
        code, out, err = run(self.port, self.out, self.appdata)
        self.assertEqual(code, 0, err + out)
        kind, path = out.split('\t', 1)
        self.assertEqual(kind, 'ok')
        return zipfile.ZipFile(path)

    def test_holds_the_logs_the_dumps_and_the_summary(self):
        with self.make() as archive:
            names = archive.namelist()
            self.assertIn('diagnostics.txt', names)
            self.assertIn('launcher-run.log', names)
            self.assertEqual(len([n for n in names if n.startswith('logs/run-')]), 10)   # the newest ten
            self.assertEqual(len([n for n in names if n.endswith('.dmp')]), 3)           # the newest three
            self.assertIn('mods.txt', names)
            text = archive.read('diagnostics.txt').decode('utf-8')
            self.assertIn('System: ', text)
            self.assertIn('launcher-run.log', text)

    def test_no_secret_leaves_in_the_settings(self):
        with self.make() as archive:
            everything = b''.join(archive.read(n) for n in archive.namelist() if n.endswith(('.json', '.ini')))
            for secret in (b'secret-token', b'hunter2', b'BBCOOP-xyz', b'abc'):
                self.assertNotIn(secret, everything)
            settings = json.loads(archive.read('settings.json'))
            self.assertEqual(settings['fps_mode'], '60')
            self.assertEqual(settings['thd_token'], '<hidden>')
            self.assertEqual(settings['coop_name'], '')           # an empty field stays empty
            self.assertIn(b'upscaler=taa', archive.read('bbport.ini'))
            self.assertIn(b'key_interact=E', archive.read('bbport.ini'))      # key bindings are not secrets

    def test_a_missing_log_folder_is_not_an_error(self):
        for path in (self.port / 'logs').iterdir():
            path.unlink()
        (self.port / 'logs').rmdir()
        with self.make() as archive:
            self.assertIn('launcher-run.log', archive.namelist())

    def test_usage_error(self):
        done = subprocess.run([sys.executable, '-I', str(SCRIPT)], capture_output=True, text=True)
        self.assertEqual(done.returncode, 2)


if __name__ == '__main__':
    unittest.main()
