"""scripts/launcher_steam.py: Add to Steam, against a fake Steam folder."""
from paths import ROOT
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_steam.py'
sys.path.insert(0, str(ROOT / 'launcher'))
import bbport_steam  # noqa: E402


def run(*args):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, done.stdout, done.stderr


def fake_steam(root):
    """userdata/1001/config and userdata/2002/config, plus the folders Steam keeps that are not accounts."""
    for account in ('1001', '2002', '0', 'anonymous'):
        (root / 'userdata' / account / 'config').mkdir(parents=True)
    return root


class StatusTests(unittest.TestCase):
    def test_counts_the_accounts_and_ignores_other_folders(self):
        with tempfile.TemporaryDirectory() as directory:
            steam = fake_steam(Path(directory))
            code, out, err = run('status', '--steam-dir', steam)
            self.assertEqual(code, 0, err)
            lines = dict(line.split('\t') for line in out.splitlines())
            self.assertEqual(lines['steam'], str(steam))
            self.assertEqual(lines['accounts'], '2')
            self.assertIn(lines['running'], ('0', '1'))

    def test_a_folder_without_accounts(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, _err = run('status', '--steam-dir', directory)
            self.assertEqual(code, 0)
            self.assertIn('accounts\t0', out)


class AddTests(unittest.TestCase):
    EXE = r'C:\Games\bb\Play Bloodborne.exe'

    def add(self, steam, *extra):
        return run('add', 'Bloodborne', self.EXE, r'C:\Games\bb', self.EXE, '--steam-dir', steam, *extra)

    def test_adds_to_every_account_then_updates_without_duplicating(self):
        with tempfile.TemporaryDirectory() as directory:
            steam = fake_steam(Path(directory))
            code, out, err = self.add(steam, '--options', '--play')
            self.assertEqual((code, out.strip()), (0, 'added'), err)
            for account in ('1001', '2002'):
                root = bbport_steam.parse((steam / 'userdata' / account / 'config' / 'shortcuts.vdf').read_bytes())[0]
                entries = [value for kind, key, value in root[0][2]]
                self.assertEqual(len(entries), 1)
                fields = {k: v for _t, k, v in entries[0]}
                self.assertEqual(fields['AppName'], 'Bloodborne')
                self.assertEqual(fields['Exe'], f'"{self.EXE}"')
                self.assertEqual(fields['LaunchOptions'], '--play')
            code, out, err = self.add(steam)
            self.assertEqual((code, out.strip()), (0, 'updated'), err)
            root = bbport_steam.parse((steam / 'userdata' / '1001' / 'config' / 'shortcuts.vdf').read_bytes())[0]
            self.assertEqual(len(root[0][2]), 1)
            self.assertTrue((steam / 'userdata' / '1001' / 'config' / 'shortcuts.vdf.bak').is_file())

    def test_other_shortcuts_are_kept(self):
        with tempfile.TemporaryDirectory() as directory:
            steam = fake_steam(Path(directory))
            vdf = steam / 'userdata' / '1001' / 'config' / 'shortcuts.vdf'
            bbport_steam.add_shortcut(vdf, 'Other', r'C:\Other\o.exe', r'C:\Other', '')
            self.assertEqual(self.add(steam)[0], 0)
            root = bbport_steam.parse(vdf.read_bytes())[0]
            names = [dict((k, v) for _t, k, v in value)['AppName'] for _kind, _key, value in root[0][2]]
            self.assertEqual(names, ['Other', 'Bloodborne'])

    def test_no_steam_or_no_account_is_exit_code_3(self):
        with tempfile.TemporaryDirectory() as directory:
            code, _out, err = self.add(directory)
            self.assertEqual(code, 3)
            self.assertIn('Steam was not found', err)

    def test_usage_error(self):
        self.assertEqual(run('add', 'only-a-name')[0], 2)
        self.assertEqual(run()[0], 2)


if __name__ == '__main__':
    unittest.main()
