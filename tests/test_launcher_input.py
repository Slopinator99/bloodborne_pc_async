"""scripts/launcher_input.py: the key_<action> bindings of bbport.ini for the native Controls tab."""
from paths import ROOT
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_input.py'
sys.path.insert(0, str(ROOT / 'scripts'))
import launcher_input  # noqa: E402


def run(*args):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, done.stdout, done.stderr


def rows(text):
    return [line.split('\t') for line in text.splitlines()]


class TableTests(unittest.TestCase):
    def test_the_action_table_is_the_games(self):
        """src/input_bindings.c is authoritative: same actions, order, labels and defaults."""
        source = (ROOT / 'src' / 'input_bindings.c').read_text(encoding='utf-8')
        body = source[source.index('static const BindInfo INFO'):]
        body = body[:body.index('};')]
        game = re.findall(r'\{"(\w+)", "([^"]+)", \w+, (NULL|"[^"]+"), (\d)\}', body)
        self.assertEqual(len(game), 29)
        expected = [(name, label, '' if key == 'NULL' else key.strip('"'), '' if mouse == '0' else f'Mouse{mouse}')
                    for name, label, key, mouse in game]
        self.assertEqual(launcher_input.ACTIONS, expected)

    def test_mouse_inputs_are_the_games(self):
        source = (ROOT / 'src' / 'input_bindings.c').read_text(encoding='utf-8')
        names = re.search(r'MOUSE_NAMES\[\] = \{NULL, ([^}]+)\}', source).group(1)
        game = [n.strip().strip('"') for n in names.split(',')]
        self.assertEqual([m.lower() for m in launcher_input.MOUSE_INPUTS], game)


class LoadTests(unittest.TestCase):
    def test_defaults_without_a_file_or_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, err = run('load', Path(directory) / 'bbport.ini')
            self.assertEqual(code, 0, err)
            table = rows(out)
            self.assertEqual(len([r for r in table if r[0] == 'action']), 29)
            binds = {r[1]: (r[2], r[3]) for r in table if r[0] == 'bind'}
            self.assertEqual(binds['interact'], ('E', ''))
            self.assertEqual(binds['attack'], ('', 'Mouse1'))
            self.assertEqual(binds['firearm'], ('X', 'Mouse2'))
            self.assertEqual(binds['roll'], ('Space', ''))

    def test_reads_lines_like_the_game(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'bbport.ini'
            ini.write_text('# comment\nupscaler=taa\nkey_interact = F\nkey_firearm=X, mouse2\nkey_gestures=none\n'
                           'key_attack = Mouse3 , Left Ctrl\nkey_bogus=Q\nkey_roll=Z\nkey_roll=V\n', encoding='utf-8')
            binds = {r[1]: (r[2], r[3]) for r in rows(run('load', ini)[1]) if r[0] == 'bind'}
            self.assertEqual(binds['interact'], ('F', ''))
            self.assertEqual(binds['firearm'], ('X', 'Mouse2'))
            self.assertEqual(binds['gestures'], ('', ''))
            self.assertEqual(binds['attack'], ('Left Ctrl', 'Mouse3'))     # either order
            self.assertEqual(binds['roll'], ('V', ''))                      # the last line wins
            self.assertNotIn('bogus', binds)

    def test_usage_error(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run('load')[0], 2)


class SaveTests(unittest.TestCase):
    def test_rewrites_in_place_appends_the_rest_and_keeps_every_other_line(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'bbport.ini'
            ini.write_text('# my settings\nupscaler=taa\nkey_interact=E\nsharpen=1\nkey_roll=Space\nkey_roll=Z\n',
                           encoding='utf-8')
            code, out, err = run('save', ini, 'interact=F', 'firearm=X,Mouse2', 'gestures=', 'roll=Left Shift')
            self.assertEqual((code, out.strip()), (0, 'ok'), err)
            lines = ini.read_text(encoding='utf-8').splitlines()
            self.assertEqual(lines[:4], ['# my settings', 'upscaler=taa', 'key_interact=F', 'sharpen=1'])
            self.assertEqual(lines.count('key_roll=Left Shift'), 1)          # the duplicate is gone
            self.assertNotIn('key_roll=Z', lines)
            self.assertIn('key_firearm=X, Mouse2', lines)
            self.assertIn('key_gestures=none', lines)
            binds = {r[1]: (r[2], r[3]) for r in rows(run('load', ini)[1]) if r[0] == 'bind'}
            self.assertEqual(binds['firearm'], ('X', 'Mouse2'))
            self.assertEqual(binds['gestures'], ('', ''))

    def test_a_mouse_input_alone_and_a_new_file(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'new.ini'
            self.assertEqual(run('save', ini, 'attack=,WheelUp')[0], 0)
            self.assertEqual(ini.read_text(encoding='utf-8'), 'key_attack=WheelUp\n')

    def test_bad_input_changes_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'bbport.ini'
            ini.write_text('key_interact=E\n', encoding='utf-8')
            for bad in ('bogus=A', 'interact=Mouse9,Mouse9'):
                code, _out, err = run('save', ini, bad)
                self.assertEqual(code, 1, bad)
                self.assertTrue(err.strip(), bad)
            self.assertEqual(ini.read_text(encoding='utf-8'), 'key_interact=E\n')

    def test_no_edits_writes_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'bbport.ini'
            self.assertEqual(run('save', ini)[0], 0)
            self.assertFalse(ini.exists())


if __name__ == '__main__':
    unittest.main()
