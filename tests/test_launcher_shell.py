"""bb-shell.h: the desktop shortcut the native launcher writes."""
from paths import ROOT
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

MSYS_CLANG = Path(os.environ.get('MSYS2_ROOT', 'C:/msys64')) / 'clang64' / 'bin'


def find_clang():
    return shutil.which('clang') or (str(MSYS_CLANG / 'clang.exe') if (MSYS_CLANG / 'clang.exe').is_file() else None)


@unittest.skipUnless(os.name == 'nt', 'bb-shell.h uses COM')
class ShortcutTests(unittest.TestCase):
    def test_writes_a_shortcut_and_reads_it_back(self):
        clang = find_clang()
        if not clang:
            self.skipTest('clang not found')
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            exe = work / 'test_launcher_shell.exe'
            build = subprocess.run([clang, '-std=c11', '-Wall', '-Wextra', '-o', str(exe),
                                    str(ROOT / 'tests' / 'test_launcher_shell.c'), '-lole32', '-luuid', '-lshell32'],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(build.returncode, 0, build.stdout)
            self.assertNotIn('warning', build.stdout)
            run = subprocess.run([str(exe), str(work)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout)
            self.assertIn('OK', run.stdout)
            self.assertTrue((work / 'Bloodborne.lnk').is_file())


if __name__ == '__main__':
    unittest.main()
