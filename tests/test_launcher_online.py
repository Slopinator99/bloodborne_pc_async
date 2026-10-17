"""scripts/launcher_online.py: The Hunter's Dream's connection test, sign-in and environment."""
from paths import ROOT
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_online.py'


def run(*args, appdata=None):
    env = dict(os.environ)
    if appdata:
        env['APPDATA'] = str(appdata)
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8', env=env, timeout=120)
    return done.returncode, done.stdout, done.stderr


class CheckTests(unittest.TestCase):
    def test_reports_each_address_of_the_game_server(self):
        code, out, err = run('check', 'http://127.0.0.1')
        self.assertEqual(code, 0, err)
        self.assertIn('Game server 127.0.0.1:20443: not reachable', out)
        self.assertIn('Co-op service 127.0.0.1:18671: not reachable', out)

    def test_usage_error(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run('check')[0], 2)
        self.assertEqual(run('signin')[0], 2)


class EnvironmentTests(unittest.TestCase):
    def settings(self, appdata, **values):
        folder = Path(appdata) / 'bbport-launcher'
        folder.mkdir(parents=True)
        (folder / 'settings.json').write_text(json.dumps(values), encoding='utf-8')

    def env_for(self, **values):
        with tempfile.TemporaryDirectory() as appdata:
            self.settings(appdata, **values)
            code, out, err = run('env', appdata=appdata)
        self.assertEqual(code, 0, err)
        return dict(line.split('=', 1) for line in out.splitlines() if line)

    def test_nothing_while_offline_or_in_co_op(self):
        self.assertEqual(self.env_for(play_mode='offline', thd_token='t', thd_online_id='me'), {})
        self.assertEqual(self.env_for(play_mode='coop', thd_token='t', thd_online_id='me'), {})
        self.assertEqual(self.env_for(), {})

    def test_hunters_dream_environment(self):
        env = self.env_for(play_mode='huntersdream', thd_online_id='Hunter', thd_token='secret',
                           online_community='https://example.org:443', online_upnp=False)
        self.assertEqual(env['BB_ONLINE'], '1')
        self.assertEqual(env['BB_NET_BACKEND'], 'thd')
        self.assertEqual(env['BB_THD_ONLINE_ID'], 'Hunter')
        self.assertEqual(env['BB_THD_TOKEN'], 'secret')
        self.assertEqual(env['BB_UPNP'], '0')
        self.assertEqual(env['BB_COMMUNITY_SERVER'], 'https://example.org')
        self.assertNotIn('BB_SHADNET_NPID', env)
        self.assertFalse(any(name.startswith('BB_GAME') for name in env))   # only the online variables

    def test_the_old_online_settings_still_start_the_hunters_dream(self):
        env = self.env_for(online=True, online_pool='huntersdream', thd_online_id='A', thd_token='b')
        self.assertEqual(env['BB_NET_BACKEND'], 'thd')


class SignInTests(unittest.TestCase):
    def sign_in(self, fake):
        sys.path.insert(0, str(ROOT / 'scripts'))
        import launcher_online
        real, launcher_online.win.thd_sign_in = launcher_online.win.thd_sign_in, fake
        try:
            return list(launcher_online.sign_in('https://example.org'))
        finally:
            launcher_online.win.thd_sign_in = real

    def test_the_code_comes_before_the_result(self):
        def fake(server, on_code, cancelled=lambda: False):
            on_code('https://example.org/link?code=AB-12', 'AB-12')
            return 'Hunter', 'token'
        self.assertEqual(self.sign_in(fake), ['code\thttps://example.org/link?code=AB-12\tAB-12',
                                              'ok\tHunter\ttoken'])

    def test_a_refusal_is_one_error_line(self):
        def fake(server, on_code, cancelled=lambda: False):
            raise ValueError('the sign-in was denied')
        self.assertEqual(self.sign_in(fake), ['error\tthe sign-in was denied'])


if __name__ == '__main__':
    unittest.main()
