"""scripts/launcher_input.py: input.ini for the native launcher's Controls tab."""
from paths import ROOT
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_input.py'
sys.path.insert(0, str(ROOT / 'launcher'))
import bbport_input_config as cfg  # noqa: E402


def run(*args):
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, done.stdout, done.stderr


def rows(text):
    return [line.split('\t') for line in text.splitlines()]


class LoadTests(unittest.TestCase):
    def test_a_missing_file_loads_the_shipped_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, err = run('load', Path(directory) / 'input.ini')
            self.assertEqual(code, 0, err)
            table = rows(out)
            binds = {r[1]: r[2] for r in table if r[0] == 'bind'}
            self.assertEqual(binds['cross'], 'space')
            self.assertEqual(binds['r3'], 'q')                       # the first of the two r3 lines
            self.assertEqual(binds['axis_left_y_minus'], 'w')
            actions = [r for r in table if r[0] == 'action']
            self.assertEqual(len(actions), len(cfg.KEYBOARD_OUTPUTS))
            self.assertEqual(actions[0][1:], ['cross', 'Cross', 'buttons'])
            self.assertEqual(actions[-1][3], 'sticks')
            mouse = [r for r in table if r[0] == 'mouse'][0]
            self.assertEqual(mouse[1:3], ['0', 'right'])
            self.assertEqual([r for r in table if r[0] == 'hotkeys'][0][1:], ['f7', 'f8'])
            reserved = [r for r in table if r[0] == 'reserved'][0][1].split(',')
            self.assertEqual(set(reserved), {'insert', 'f9', 'f7', 'f8'})
            keys = [r for r in table if r[0] == 'keys'][0][1].split(',')
            self.assertIn('space', keys)
            self.assertIn('leftbutton', keys)

    def test_usage_error(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run('load')[0], 2)


class SaveTests(unittest.TestCase):
    def test_edits_replace_first_bindings_and_keep_everything_else(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.ini'
            path.write_text(cfg.DEFAULT_INPUT_INI, encoding='utf-8')
            code, out, err = run('save', path, 'cross=x', 'circle=rightbutton', 'r3=', 'axis_left_x_plus=right')
            self.assertEqual(code, 0, err)
            text = path.read_text(encoding='utf-8')
            self.assertIn('cross = x', text)
            self.assertIn('circle = rightbutton', text)
            self.assertIn('axis_left_x_plus = right', text)
            self.assertNotIn('r3 = q', text)                      # cleared
            self.assertIn('r3 = c', text)                         # the second binding is untouched
            self.assertIn('cross = cross', text)                  # the controller line is untouched
            self.assertIn('hotkey_toggle_mouse_to_joystick = f7', text)
            self.assertIn('# Controller', text)                   # comments kept
            # And the helper reads back what it wrote.
            binds = {r[1]: r[2] for r in rows(run('load', path)[1]) if r[0] == 'bind'}
            self.assertEqual((binds['cross'], binds['circle'], binds['axis_left_x_plus']), ('x', 'rightbutton', 'right'))

    def test_mouse_look_is_written_as_the_python_launcher_does(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.ini'
            path.write_text(cfg.DEFAULT_INPUT_INI, encoding='utf-8')
            code, _out, err = run('save', path, '--mouse', '1,left,2.5,0.25')
            self.assertEqual(code, 0, err)
            text = path.read_text(encoding='utf-8')
            self.assertIn('\nmouse_to_joystick = left\n', text)
            self.assertIn('mouse_movement_params = 0.5, 2.5, 0.25', text)
            mouse = [r for r in rows(run('load', path)[1]) if r[0] == 'mouse'][0]
            self.assertEqual(mouse[1:5], ['1', 'left', '2.5', '0.25'])
            run('save', path, '--mouse', '0,right,1,0.125')
            self.assertIn('# mouse_to_joystick = right', path.read_text(encoding='utf-8'))

    def test_a_stick_direction_refuses_a_mouse_button_and_bad_names_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.ini'
            path.write_text(cfg.DEFAULT_INPUT_INI, encoding='utf-8')
            before = path.read_text(encoding='utf-8')
            for bad in ('axis_left_x_plus=leftbutton', 'cross=notakey', 'bogus=a'):
                code, _out, err = run('save', path, bad)
                self.assertEqual(code, 1, bad)
                self.assertTrue(err.strip(), bad)
            self.assertEqual(path.read_text(encoding='utf-8'), before)   # nothing written

    def test_saving_without_edits_does_not_create_the_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.ini'
            self.assertEqual(run('save', path)[0], 0)
            self.assertFalse(path.exists())


if __name__ == '__main__':
    unittest.main()
