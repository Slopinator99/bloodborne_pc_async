"""The two Windows launchers must agree on the logging/async defaults."""
from paths import ROOT
import re
import unittest


class LauncherDefaultsTests(unittest.TestCase):
    def py_defaults(self):
        text = (ROOT / 'launcher' / 'bbport_launcher_win.py').read_text(encoding='utf-8')
        block = re.search(r'APP_DEFAULTS = \{(.*?)\}', text, re.S).group(1)
        return {k: v for k, v in re.findall(r"'(\w+)':\s*(True|False)", block)}

    def c_defaults(self):
        text = (ROOT / 'packaging' / 'windows' / 'bb-launcher.c').read_text(encoding='utf-8')
        return {key: default for key, default in
                re.findall(r'app_get\("(frame_stats|async_pipelines)",\s*"launcher_\w+",\s*v,\s*sizeof\(v\),\s*"(\d)"\)', text)}

    def test_frame_stats_default_matches(self):
        c = self.c_defaults()
        self.assertEqual(self.py_defaults()['frame_stats'] == 'True', c['frame_stats'] == '1')

    def test_async_default_matches(self):
        c = self.c_defaults()
        self.assertEqual(self.py_defaults()['async_pipelines'] == 'True', c['async_pipelines'] == '1')

    def test_c_launcher_persists_both(self):
        text = (ROOT / 'packaging' / 'windows' / 'bb-launcher.c').read_text(encoding='utf-8')
        self.assertIn('cfg_set_bool("async_pipelines"', text)
        self.assertIn('cfg_set_bool("frame_stats"', text)


if __name__ == '__main__':
    unittest.main()
