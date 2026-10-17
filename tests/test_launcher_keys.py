"""bb-keys.h: every name the native launcher can produce is one SDL (and so the game) accepts."""
from paths import ROOT
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

MSYS_CLANG = Path(os.environ.get('MSYS2_ROOT', 'C:/msys64')) / 'clang64' / 'bin'


def tool(name):
    """The MSYS2 CLANG64 tool when there is one (the build's own compiler and SDL), else the PATH's."""
    local = MSYS_CLANG / f'{name}.exe'
    return str(local) if local.is_file() else shutil.which(name)


@unittest.skipUnless(os.name == 'nt', 'bb-keys.h uses the Windows key codes')
class KeyNameTests(unittest.TestCase):
    def table(self):
        clang, pkg = tool('clang'), tool('pkg-config')
        if not clang or not pkg:
            self.skipTest('clang or pkg-config not found')
        prefix = MSYS_CLANG.parent
        env = dict(os.environ, PATH=str(MSYS_CLANG) + os.pathsep + os.environ['PATH'],
                   PKG_CONFIG_PATH=os.pathsep.join([str(prefix / 'lib' / 'pkgconfig'), str(prefix / 'share' / 'pkgconfig')]))
        flags = subprocess.run([pkg, '--cflags', '--libs', 'sdl3'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, env=env)
        if flags.returncode:
            self.skipTest('SDL3 not found')
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / 'keys.exe'
            build = subprocess.run([clang, '-std=c11', '-Wall', '-Wextra', '-o', str(exe),
                                    str(ROOT / 'tests' / 'test_launcher_keys.c'), *flags.stdout.split()],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
            self.assertEqual(build.returncode, 0, build.stdout)
            self.assertNotIn('warning', build.stdout)
            run = subprocess.run([str(exe)], stdout=subprocess.PIPE, text=True, env=env)
        self.assertEqual(run.returncode, 0, 'SDL rejected a name:\n' + '\n'.join(
            line for line in run.stdout.splitlines() if line.startswith('BAD')))
        names = {}
        for line in run.stdout.splitlines():
            vk, flag, name = line.split(None, 2)
            names[(vk, flag)] = name
        return names

    def test_sdl_knows_every_name(self):
        names = self.table()
        self.assertGreater(len(names), 90)

    def test_spot_checks(self):
        names = self.table()
        self.assertEqual(names[('65', '0')], 'A')
        self.assertEqual(names[('116', '0')], 'F5')
        self.assertEqual(names[('32', '0')], 'Space')
        self.assertEqual(names[('13', '0')], 'Return')
        self.assertEqual(names[('13', '1')], 'Keypad Enter')
        self.assertEqual(names[('37', '0')], 'Left')
        self.assertEqual(names[('96', '0')], 'Keypad 0')
        self.assertEqual(names[('192', '0')], '`')
        self.assertEqual(names[('17', '1')], 'Right Ctrl')
        self.assertEqual(names[('18', '0')], 'Left Alt')
        self.assertEqual(names[('16', 'scan36')], 'Right Shift')
        self.assertEqual(names[('16', 'scan2a')], 'Left Shift')

    def test_the_games_default_keys_can_all_be_typed(self):
        """Every default key of the game's table (launcher_input.ACTIONS) has a Windows key."""
        import sys
        sys.path.insert(0, str(ROOT / 'scripts'))
        import launcher_input
        typed = set(self.table().values())
        defaults = {key for _n, _l, key, _m in launcher_input.ACTIONS if key}
        self.assertEqual(sorted(defaults - typed), [])


if __name__ == '__main__':
    unittest.main()
