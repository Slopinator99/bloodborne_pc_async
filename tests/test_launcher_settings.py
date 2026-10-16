"""bb-settings.h (the C launcher's reader/writer for the Python launcher's settings.json)."""
from paths import ROOT
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

MSYS_CLANG = Path(os.environ.get('MSYS2_ROOT', 'C:/msys64')) / 'clang64' / 'bin'


def find_clang():
    return shutil.which('clang') or (str(MSYS_CLANG / 'clang.exe') if (MSYS_CLANG / 'clang.exe').is_file() else None)


@unittest.skipUnless(os.name == 'nt', 'bb-settings.h uses the Windows API')
class SettingsStoreTests(unittest.TestCase):
    def test_round_trip_keeps_unknown_and_nested_values(self):
        clang = find_clang()
        if not clang:
            self.skipTest('clang not found')
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            # Written the way the Python launcher does: json.dumps(indent=2, ensure_ascii=False).
            (data / 'fixture.json').write_text(json.dumps({
                'game_dir': 'F:\\games\\CUSA03173', 'player_name': 'Chasseur \u00e9t\u00e9 \U0001F319',
                'mods_enabled': True, 'frame_stats': False, 'language': '1', 'nothing': None,
                'online_password': 'p"w\\d', 'ratio': 0.5,
                'remap': {'a': [1, 2, {'b': '}'}], 'c': None},
            }, indent=2, ensure_ascii=False), encoding='utf-8')
            (data / 'corrupt.json').write_text('{\n  "game_dir": "F:\\\\x",\n  "oops": ', encoding='utf-8')
            exe = data / 'test_launcher_settings.exe'
            build = subprocess.run([clang, '-std=c11', '-Wall', '-Wextra', '-o', str(exe),
                                    str(ROOT / 'tests' / 'test_launcher_settings.c')],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(build.returncode, 0, build.stdout)
            self.assertNotIn('warning', build.stdout)
            run = subprocess.run([str(exe), str(data)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True)
            self.assertEqual(run.returncode, 0, run.stdout)
            self.assertIn('OK', run.stdout)
            # What the C side saved is still valid JSON for the Python launcher, secrets intact.
            saved = json.loads((data / 'fixture.json').read_text(encoding='utf-8'))
            self.assertEqual(saved['online_password'], 'p"w\\d')
            self.assertEqual(saved['remap'], {'a': [1, 2, {'b': '}'}], 'c': None})
            self.assertEqual(saved['player_name'], 'Chasseur \u00e9t\u00e9 \U0001F319')
            self.assertIs(saved['frame_stats'], True)
            self.assertEqual(saved['game_dir'], 'E:\\My "Games"\\CUSA03173')
            self.assertTrue((data / 'corrupt.json.bak').is_file())


if __name__ == '__main__':
    unittest.main()
