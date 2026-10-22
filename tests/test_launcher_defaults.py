"""The C launcher (bb-launcher.c) and the Python launcher (bbport_launcher_win.py) share
bbport.ini and settings.json: every field of the C launcher must exist in the Python one, and
their defaults may only differ where that is deliberate."""
from paths import ROOT
import re
import sys
import unittest

C_SOURCE = ROOT / 'packaging' / 'windows' / 'bb-launcher.c'
FIELD = re.compile(r'FLD_(CHECK|COMBO|TEXT|FOLDER|SLIDER|PASSWORD)\(PG_\w+,\s*(ST_INI|ST_APP),\s*"(\w+)",(.*?)\),?\n', re.S)

# Where the C launcher starts differently on purpose: it defaults to the TAA setup that needs no
# downloaded FSR 4 assets, a 60 FPS cap and the intro skipped (the game's own AA is off with TAA).
KNOWN_DIFFERENCES = {
    ('ST_INI', 'upscaler'): ('taa', 'dlss'),
    ('ST_INI', 'preset'): ('0', '2'),
    ('ST_INI', 'output_res'): ('1920x1080', '2560x1440'),
    ('ST_INI', 'skip_intro'): ('1', '0'),
    ('ST_INI', 'effect_game_aa'): ('0', '1'),
    ('ST_APP', 'fps_mode'): ('60', '120'),
    ('ST_APP', 'play_mode'): ('offline', ''),  # the Python launcher derives it from older settings
}


def c_fields():
    """(store, key) -> (default text, kind), from the field table of bb-launcher.c."""
    text = C_SOURCE.read_text(encoding='utf-8')
    fields = {}
    for kind, store, key, rest in FIELD.findall(text):
        quoted = re.findall(r'"([^"]*)"', rest)
        fields[(store, key)] = ('' if kind == 'FOLDER' else quoted[-1], kind)
    return fields


def python_defaults():
    launcher = ROOT / 'launcher'
    sys.path.insert(0, str(launcher))
    argv, sys.argv = sys.argv, ['bbport_launcher_win']
    try:
        import bbport_launcher_win as win
    finally:
        sys.argv = argv
        sys.path.remove(str(launcher))
    return {'ST_INI': win.INI_DEFAULTS, 'ST_APP': win.APP_DEFAULTS}


def as_text(value):
    return '1' if value is True else '0' if value is False else str(value)


class LauncherDefaultsTests(unittest.TestCase):
    def test_table_is_found(self):
        self.assertGreater(len(c_fields()), 40)

    def test_every_c_field_exists_in_the_python_launcher(self):
        python = python_defaults()
        missing = [f'{store}:{key}' for (store, key) in c_fields() if key not in python[store]]
        self.assertEqual(missing, [], 'keys the Python launcher does not know (renamed or typo?)')

    def test_defaults_match_except_where_deliberate(self):
        python = python_defaults()
        differing = {}
        for (store, key), (default, kind) in c_fields().items():
            if kind == 'FOLDER' or key not in python[store]:
                continue  # folder defaults depend on where the port is installed
            theirs = as_text(python[store][key])
            if default != theirs:
                differing[(store, key)] = (default, theirs)
        self.assertEqual(differing, KNOWN_DIFFERENCES)

    def test_c_launcher_persists_every_app_field(self):
        text = C_SOURCE.read_text(encoding='utf-8')
        self.assertIn('cfg_set_bool(fl->key', text)
        self.assertIn('cfg_set_str(fl->key', text)
        self.assertIn('cfg_set_bool("gpu_profile"', text)

    def test_frame_stats_default_on_in_both(self):
        self.assertEqual(c_fields()[('ST_APP', 'frame_stats')][0], '1')
        self.assertIs(python_defaults()['ST_APP']['frame_stats'], True)


if __name__ == '__main__':
    unittest.main()
