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


# The drop-down lists of the C launcher against the Python launcher's own (key -> list name).
OPTION_LISTS = {
    'upscaler': 'UPSCALERS', 'preset': 'PRESETS', 'output_res': 'OUTPUTS', 'live_resolution': 'LIVE',
    'model_lod': 'LODS', 'fps_mode': 'FPS_MODES', 'frame_cap': 'FRAME_CAPS', 'frames_ahead': 'FRAMES_AHEAD',
    'present_mode': 'PRESENT_MODES', 'language': 'LANGUAGES', 'draw_pipe': 'DRAW_PIPE',
    'readbacks': 'READBACKS', 'mouse_sensitivity': 'MOUSE_SPEEDS', 'move_smoothing': 'MOVE_SMOOTHING',
    'vram_budget_mb': 'VRAM_BUDGETS', 'frame_gen': 'FRAME_GENERATION',
    'frame_gen_multiplier': 'FRAME_GEN_MULTIPLIERS', 'hud_position': 'HUD_POSITIONS',
    'fullscreen_mode': 'FULLSCREEN_MODES', 'display': 'DISPLAYS', 'voice_language': 'VOICE_LANGUAGES',
    'atlas_npc_mode': 'ATLAS_NPC_MODES', 'audio_channels': 'AUDIO_CHANNELS', 'pad_back': 'PAD_BACK',
}
# Lists the C launcher keeps shorter on purpose: key -> the values it leaves out.
SHORTER_LISTS = {}
COMBO = re.compile(r'FLD_COMBO\(PG_\w+,\s*ST_\w+,\s*"(\w+)",\s*"[^"]*",\s*"[^"]*",\s*(\w+),')
ARRAY = re.compile(r'static const char\* (\w+)\[\] = \{(.*?)\};', re.S)


def c_option_values():
    """key -> the values of its drop-down in bb-launcher.c."""
    text = C_SOURCE.read_text(encoding='utf-8')
    arrays = {name: re.findall(r'"([^"]*)"', body) for name, body in ARRAY.findall(text)}
    return {key: arrays[values] for key, values in COMBO.findall(text)}


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

    def test_drop_downs_offer_the_python_launchers_choices(self):
        sys.path.insert(0, str(ROOT / 'launcher'))
        argv, sys.argv = sys.argv, ['bbport_launcher_win']
        try:
            import bbport_launcher_win as win
        finally:
            sys.argv = argv
        problems = []
        for key, values in c_option_values().items():
            if key not in OPTION_LISTS:
                continue
            theirs = [value for value, _labels in getattr(win, OPTION_LISTS[key])]
            left_out = set(SHORTER_LISTS.get(key, ()))
            expected = [value for value in theirs if value not in left_out]
            if values != expected:
                problems.append(f'{key}: C has {values}, Python has {theirs}')
        self.assertEqual(problems, [])

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
