"""bb-keys.h: every name the native launcher can produce is one the game's input.ini accepts."""
from paths import ROOT
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(ROOT / 'launcher'))
import bbport_input_config as cfg  # noqa: E402

MSYS_CLANG = Path(os.environ.get('MSYS2_ROOT', 'C:/msys64')) / 'clang64' / 'bin'


def find_clang():
    return shutil.which('clang') or (str(MSYS_CLANG / 'clang.exe') if (MSYS_CLANG / 'clang.exe').is_file() else None)


@unittest.skipUnless(os.name == 'nt', 'bb-keys.h uses the Windows key codes')
class KeyNameTests(unittest.TestCase):
    def table(self):
        clang = find_clang()
        if not clang:
            self.skipTest('clang not found')
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / 'keys.exe'
            build = subprocess.run([clang, '-std=c11', '-Wall', '-Wextra', '-o', str(exe),
                                    str(ROOT / 'tests' / 'test_launcher_keys.c')],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(build.returncode, 0, build.stdout)
            self.assertNotIn('warning', build.stdout)
            run = subprocess.run([str(exe)], stdout=subprocess.PIPE, text=True, check=True)
        names = {}
        for line in run.stdout.splitlines():
            vk, flag, name = line.split()
            names[(vk, flag)] = name
        return names

    def test_every_name_is_one_the_game_accepts(self):
        names = self.table()
        self.assertGreater(len(names), 90)
        unknown = sorted({name for name in names.values() if name not in cfg.KEY_NAMES_SET})
        self.assertEqual(unknown, [])

    def test_spot_checks(self):
        names = self.table()
        self.assertEqual(names[('65', '0')], 'a')          # 'A'
        self.assertEqual(names[('48', '0')], '0')
        self.assertEqual(names[('116', '0')], 'f5')        # VK_F5
        self.assertEqual(names[('32', '0')], 'space')
        self.assertEqual(names[('13', '0')], 'enter')
        self.assertEqual(names[('13', '1')], 'kpenter')    # numpad Enter is the extended Enter
        self.assertEqual(names[('37', '0')], 'left')
        self.assertEqual(names[('96', '0')], 'kp0')
        self.assertEqual(names[('192', '0')], 'grave')
        self.assertEqual(names[('17', '1')], 'rctrl')
        self.assertEqual(names[('18', '0')], 'lalt')
        self.assertEqual(names[('36', '0')], 'home')
        self.assertEqual(names[('16', 'scan36')], 'rshift')
        self.assertEqual(names[('16', 'scan2a')], 'lshift')

    def test_most_of_the_game_names_are_reachable_from_a_standard_keyboard(self):
        produced = set(self.table().values())
        # Only a Linux/Mac key or a rare one is out of reach: lmeta/rmeta, kpcomma, kpequals.
        unreachable = sorted(cfg.KEY_NAMES_SET - produced)
        self.assertEqual(unreachable, ['kpcomma', 'kpequals', 'lmeta', 'rmeta'])


if __name__ == '__main__':
    unittest.main()
