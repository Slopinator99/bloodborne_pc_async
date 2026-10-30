"""scripts/launcher_input.py: the key_<action> bindings of bbport.ini for the native Controls tab,
on launcher/bbport_bindings.py (three bindings per action, key combinations)."""
from paths import ROOT
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_input.py'
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'launcher'))
import bbport_bindings  # noqa: E402
import launcher_input  # noqa: E402


def run(*args):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, done.stdout, done.stderr


def table(text):
    rows = [line.split('\t') for line in text.splitlines()]
    return ([r for r in rows if r[0] == 'action'], {r[1]: r[2:] for r in rows if r[0] == 'bind'})


class EngineTableTests(unittest.TestCase):
    def test_the_action_table_is_the_games(self):
        """src/input_bindings.c is authoritative: same actions in the same order, same defaults."""
        source = (ROOT / 'src' / 'input_bindings.c').read_text(encoding='utf-8')
        body = source[source.index('static const BindInfo INFO'):]
        body = body[:body.index('};')]
        game = re.findall(r'\{"(\w+)", "[^"]+", \w+, (NULL|"[^"]+"), (\d)\}', body)
        self.assertEqual(len(game), 29)
        expected = []
        for name, key, mouse in game:
            defaults = ([[key.strip('"')]] if key != 'NULL' else []) + ([[f'Mouse{mouse}']] if mouse != '0' else [])
            expected.append((name, defaults))
        self.assertEqual([(a, d) for a, _l, d in bbport_bindings.ACTIONS], expected)

    def test_the_slot_and_combination_limits_are_the_games(self):
        source = (ROOT / 'src' / 'input_bindings.c').read_text(encoding='utf-8')
        header = (ROOT / 'src' / 'input_bindings.h').read_text(encoding='utf-8')
        text = source + header
        self.assertEqual(int(re.search(r'#define BIND_SLOTS (\d+)', text).group(1)), bbport_bindings.SLOTS)
        self.assertEqual(int(re.search(r'#define BIND_COMBO_PARTS (\d+)', text).group(1)), bbport_bindings.COMBO_PARTS)

    def test_mouse_inputs_are_the_games(self):
        source = (ROOT / 'src' / 'input_bindings.c').read_text(encoding='utf-8')
        names = re.search(r'MOUSE_NAMES\[\] = \{NULL, ([^}]+)\}', source).group(1)
        game = [n.strip().strip('"') for n in names.split(',')]
        self.assertEqual([m.lower() for m in bbport_bindings.MOUSE_NAMES], game)


class LoadTests(unittest.TestCase):
    def test_defaults_without_a_file_or_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, err = run('load', Path(directory) / 'bbport.ini')
            self.assertEqual(code, 0, err)
            actions, binds = table(out)
            self.assertEqual(len(actions), 29)
            self.assertEqual(binds['interact'], ['E', '', ''])
            self.assertEqual(binds['attack'], ['Mouse1', '', ''])
            self.assertEqual(binds['firearm'], ['X', 'Mouse2', ''])
            self.assertEqual(binds['transform'], ['C', 'Mouse3', ''])

    def test_reads_three_bindings_and_combinations(self):
        with tempfile.TemporaryDirectory() as directory:
            ini = Path(directory) / 'bbport.ini'
            ini.write_text('# comment\nupscaler=taa\nkey_interact = F\nkey_firearm=X, Mouse2, WheelUp\n'
                           'key_strong_attack=Left Shift+Mouse1\nkey_gestures=none\n', encoding='utf-8')
            _a, binds = table(run('load', ini)[1])
            self.assertEqual(binds['interact'], ['F', '', ''])
            self.assertEqual(binds['firearm'], ['X', 'Mouse2', 'WheelUp'])
            self.assertEqual(binds['strong_attack'], ['Left Shift+Mouse1', '', ''])
            self.assertEqual(binds['gestures'], ['', '', ''])


class SaveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.ini = Path(self.temporary.name) / 'bbport.ini'

    def test_only_what_differs_from_the_defaults_is_written_and_other_lines_stay(self):
        self.ini.write_text('upscaler=taa\nkey_interact=F\nkey_roll=Space\n[x]\n', encoding='utf-8')
        code, _out, err = run('save', self.ini, 'interact=E', 'roll=Left Shift+Space, Mouse4', 'firearm=X,Mouse2',
                              'gestures=none')
        self.assertEqual(code, 0, err)
        lines = self.ini.read_text(encoding='utf-8').splitlines()
        self.assertEqual(lines[:2], ['upscaler=taa', '[x]'])
        self.assertIn('key_roll=Left Shift+Space, Mouse4', lines)
        self.assertIn('key_gestures=none', lines)
        self.assertNotIn('key_interact=E', lines)          # the default: no line
        self.assertFalse([l for l in lines if l.startswith('key_firearm')])

    def test_a_round_trip_keeps_three_bindings(self):
        value = 'X, Mouse2, WheelUp'
        self.assertEqual(run('save', self.ini, f'firearm={value}')[0], 0)
        _a, binds = table(run('load', self.ini)[1])
        self.assertEqual(binds['firearm'], ['X', 'Mouse2', 'WheelUp'])

    def test_a_fourth_binding_is_dropped_like_the_game_does(self):
        run('save', self.ini, 'firearm=A, B, C, D')
        _a, binds = table(run('load', self.ini)[1])
        self.assertEqual(binds['firearm'], ['A', 'B', 'C'])

    def test_unknown_actions_and_mouse_inputs_are_refused(self):
        self.assertEqual(run('save', self.ini, 'nothing=E')[0], 1)
        self.assertEqual(run('save', self.ini, 'attack=Mouse9')[0], 1)

    def test_reset_removes_every_key_line_and_keeps_the_rest(self):
        self.ini.write_text('upscaler=taa\nkey_roll=Q\nkey_attack=none\n', encoding='utf-8')
        self.assertEqual(run('reset', self.ini)[0], 0)
        self.assertEqual(self.ini.read_text(encoding='utf-8').strip(), 'upscaler=taa')

    def test_usage_error(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run('load')[0], 2)


if __name__ == '__main__':
    unittest.main()
