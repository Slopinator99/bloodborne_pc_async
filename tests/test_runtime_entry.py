"""launcher/bb_runtime.py: the entry point of the frozen bb-runtime.exe."""
from paths import ROOT
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(ROOT / 'launcher'))
argv, sys.argv = sys.argv, ['bb_runtime']
import bb_runtime  # noqa: E402
sys.argv = argv


class EntryTests(unittest.TestCase):
    def test_no_arguments_start_the_native_launcher_when_it_is_next_to_the_runtime(self):
        with tempfile.TemporaryDirectory() as directory:
            here = Path(directory)
            self.assertIsNone(bb_runtime.native_launcher([], True, here))      # not there yet
            (here / 'bb-launcher.exe').write_bytes(b'MZ')
            self.assertEqual(bb_runtime.native_launcher([], True, here), here / 'bb-launcher.exe')

    def test_every_other_use_is_left_to_the_runtime(self):
        with tempfile.TemporaryDirectory() as directory:
            here = Path(directory)
            (here / 'bb-launcher.exe').write_bytes(b'MZ')
            for args in (['--run'], ['--play'], ['--script', 'x.py'], ['--install-update', 'a', '1']):
                self.assertIsNone(bb_runtime.native_launcher(args, True, here), args)
            # A source tree keeps its own window.
            self.assertIsNone(bb_runtime.native_launcher([], False, here))

    def test_the_package_is_built_from_this_entry_point(self):
        script = (ROOT / 'packaging' / 'windows' / 'package.sh').read_text(encoding='utf-8')
        self.assertIn('launcher/bb_runtime.py', script)
        self.assertIsNone(re.search(r'PWD/launcher/bbport_launcher_win\.py', script),
                          'PyInstaller must not freeze the launcher module as its entry script')


if __name__ == '__main__':
    unittest.main()
