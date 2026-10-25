"""packaging/windows/script_imports.py: the standard modules the scripts import go into the package."""
from paths import ROOT
import subprocess
import sys
import unittest

sys.path.insert(0, str(ROOT / 'packaging' / 'windows'))
import script_imports  # noqa: E402


class ScriptImportsTests(unittest.TestCase):
    def test_finds_plain_from_and_nested_imports(self):
        found = script_imports.imported_modules(
            'import platform, os.path\nfrom shutil import copy\ndef f():\n    import zipfile\n    from . import sibling\n')
        self.assertEqual(found, {'platform', 'os.path', 'shutil', 'zipfile'})

    def test_only_the_standard_library_is_listed(self):
        modules = script_imports.standard_library_imports([ROOT / 'scripts'])
        self.assertIn('platform', modules)       # runlog.py: the module that once kept the packaged game from starting
        self.assertIn('zipfile', modules)
        self.assertNotIn('PIL', modules)         # third party: package.sh installs it itself
        self.assertNotIn('runlog', modules)      # the scripts' own modules ride along as files

    def test_the_command_line_prints_one_module_per_line(self):
        done = subprocess.run([sys.executable, str(ROOT / 'packaging' / 'windows' / 'script_imports.py'),
                               str(ROOT / 'run.py')], capture_output=True, text=True, timeout=60)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn('subprocess', done.stdout.split())

    def test_every_script_import_is_covered_by_the_scan(self):
        """What runlog.py imports must be in the list package.sh gives PyInstaller."""
        listed = set(script_imports.standard_library_imports([ROOT / 'scripts', ROOT / 'run.py']))
        for needed in script_imports.imported_modules((ROOT / 'scripts' / 'runlog.py').read_text(encoding='utf-8')):
            if needed.split('.')[0] in sys.stdlib_module_names:
                self.assertIn(needed, listed)


if __name__ == '__main__':
    unittest.main()
