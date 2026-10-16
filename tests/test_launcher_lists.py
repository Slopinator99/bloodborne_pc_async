"""scripts/launcher_lists.py: the mod and patch lists the native launcher shows."""
from paths import ROOT
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_lists.py'

PATCH_XML = '''<?xml version="1.0" encoding="utf-8"?>
<Patch>
  <TitleID><ID>CUSA03173</ID></TitleID>
  <Metadata Title="Bloodborne" Name="{name}" Author="{author}" AppVer="{version}" AppElf="eboot.bin"
            isEnabled="{enabled}">
    <PatchList><Line Type="bytes" Address="0x400000" Value="90"/></PatchList>
  </Metadata>
</Patch>
'''


def run(mods, patches, *extra):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), str(mods), str(patches), *extra],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, done.stdout.splitlines(), done.stderr


class LauncherListsTests(unittest.TestCase):
    def test_lists_mods_and_patches_in_the_agreed_line_format(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            mods, patches = root / 'mods', root / 'patches'
            (mods / 'Zeta' / 'dvdroot_ps4').mkdir(parents=True)
            (mods / 'alpha' / 'chr').mkdir(parents=True)       # the game's folders directly
            (mods / 'Not a mod').mkdir()                        # no recognisable layout
            patches.mkdir()
            (patches / 'one.xml').write_text(PATCH_XML.format(
                name='Wide\tFOV', author='Ann\nB', version='01.09', enabled='true'), encoding='utf-8')
            (patches / 'two.xml').write_text(PATCH_XML.format(
                name='Other', author='', version='01.09', enabled='false'), encoding='utf-8')
            (patches / 'old.xml').write_text(PATCH_XML.format(
                name='Wrong version', author='x', version='01.05', enabled='true'), encoding='utf-8')
            code, out, err = run(mods, patches)
            self.assertEqual(code, 0, err)
            self.assertEqual(out, ['mod\talpha', 'mod\tZeta',
                                   'patch\tone.xml/Wide FOV\tAnn B\t1',
                                   'patch\ttwo.xml/Other\t\t0'])

    def test_missing_folders_give_an_empty_list(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, err = run(Path(directory) / 'no-mods', Path(directory) / 'no-patches')
            self.assertEqual((code, out), (0, []), err)

    def test_usage_error(self):
        done = subprocess.run([sys.executable, '-I', str(SCRIPT)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
        self.assertEqual(done.returncode, 2)


if __name__ == '__main__':
    unittest.main()
