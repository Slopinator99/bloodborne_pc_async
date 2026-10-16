"""bb-launcher.exe --install-update: the downloaded version installing itself over the old folder."""
from paths import ROOT
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

EXE = ROOT / 'out' / 'bb-launcher.exe'


@unittest.skipUnless(os.name == 'nt' and EXE.is_file(), 'needs a built out/bb-launcher.exe (Windows)')
class InstallModeTests(unittest.TestCase):
    def run_install(self, package, target, pid=None):
        env = dict(os.environ, BB_UPDATE_NO_RESTART='1')
        return subprocess.run([str(package / 'bb-launcher.exe'), '--install-update', str(target),
                               str(pid or 99999999)], env=env, timeout=120).returncode

    def make(self, root):
        package, target = root / 'package', root / 'install'
        package.mkdir()
        shutil.copy2(EXE, package / 'bb-launcher.exe')
        (package / 'data').mkdir()
        (package / 'data' / 'new.txt').write_text('fresh')
        (package / 'user').mkdir()
        (package / 'user' / 'save.dat').write_text('PACKAGE SAVE')
        (package / 'bbport.ini').write_text('package settings')
        (target / 'user').mkdir(parents=True)
        (target / 'user' / 'save.dat').write_text('MY SAVE')
        (target / 'mods' / 'mine').mkdir(parents=True)
        (target / 'mods' / 'mine' / 'f.txt').write_text('my mod')
        for name in ('bbport.ini', 'input.ini', 'mods.json', 'patches.json', 'last_run.log', 'launcher-run.log'):
            (target / name).write_text('mine: ' + name)
        (target / 'bb-launcher.exe').write_text('OLD')
        (target / 'obsolete.txt').write_text('stays: the installer only adds and replaces')
        return package, target

    def test_installs_over_the_old_folder_and_keeps_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            package, target = self.make(Path(directory))
            self.assertEqual(self.run_install(package, target), 0)
            self.assertEqual((target / 'data' / 'new.txt').read_text(), 'fresh')
            self.assertEqual((target / 'bb-launcher.exe').read_bytes(), EXE.read_bytes())
            self.assertEqual((target / 'user' / 'save.dat').read_text(), 'MY SAVE')
            self.assertEqual((target / 'mods' / 'mine' / 'f.txt').read_text(), 'my mod')
            for name in ('bbport.ini', 'input.ini', 'mods.json', 'patches.json', 'last_run.log', 'launcher-run.log'):
                self.assertEqual((target / name).read_text(), 'mine: ' + name, name)

    def test_refuses_to_install_a_folder_over_itself(self):
        with tempfile.TemporaryDirectory() as directory:
            package, _target = self.make(Path(directory))
            self.assertEqual(self.run_install(package, package), 2)

    def test_waits_for_the_old_launcher_to_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            package, target = self.make(Path(directory))
            old = subprocess.Popen(['powershell', '-NoProfile', '-Command', 'Start-Sleep 3'])
            try:
                import time
                start = time.time()
                self.assertEqual(self.run_install(package, target, old.pid), 0)
                self.assertGreater(time.time() - start, 1.5, 'it did not wait for the old process')
            finally:
                old.kill()


if __name__ == '__main__':
    unittest.main()
