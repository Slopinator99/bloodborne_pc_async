"""scripts/launcher_online.py: the connection test, the online environment and the bridge files."""
from paths import ROOT
import http.server
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_online.py'


def run(*args, appdata=None):
    env = dict(os.environ)
    if appdata:
        env['APPDATA'] = str(appdata)
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *map(str, args)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8', env=env, timeout=120)
    return done.returncode, done.stdout, done.stderr


class StatusHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'{"ok": true}' if self.path == '/status' else b'{}'
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


class CheckTests(unittest.TestCase):
    def test_reports_each_address_reachable_or_not(self):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen(5)
        web = http.server.ThreadingHTTPServer(('127.0.0.1', 0), StatusHandler)
        threading.Thread(target=web.serve_forever, daemon=True).start()
        try:
            server = f'127.0.0.1:{listener.getsockname()[1]}'
            webapi = f'http://127.0.0.1:{web.server_address[1]}'
            with tempfile.TemporaryDirectory() as saves:
                code, out, err = run('check', server, webapi, 'http://localhost', 'shadnet', saves, '1')
        finally:
            web.shutdown()
            web.server_close()
            listener.close()
        self.assertEqual(code, 0, err)
        self.assertIn(f'Server {server}: reachable', out)
        self.assertIn(f'WebAPI {webapi}: reachable', out)
        self.assertIn('Game server http://localhost: not reachable', out)    # nothing listens on 20443
        self.assertNotIn('Play online', out)

    def test_an_unreachable_server_and_play_online_off(self):
        with tempfile.TemporaryDirectory() as saves:
            code, out, err = run('check', '127.0.0.1:1', 'http://127.0.0.1:1', 'http://localhost', 'shadnet', saves, '0')
        self.assertEqual(code, 0, err)
        self.assertIn('Server 127.0.0.1:1: not reachable', out)
        self.assertIn('WebAPI http://127.0.0.1:1: not reachable', out)
        self.assertIn('"Play online" is off', out)

    def test_a_webapi_box_holding_the_game_server_is_ignored(self):
        with tempfile.TemporaryDirectory() as saves:
            code, out, _err = run('check', '127.0.0.1:1', 'https://thehuntersdream.com', 'https://thehuntersdream.com',
                                  'shadnet', saves, '1')
        self.assertEqual(code, 0)
        self.assertIn('is ignored', out)
        self.assertIn('WebAPI http://127.0.0.1:31315', out)     # the server address with port 31315

    def test_usage_error(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run('check', 'only-one')[0], 2)


class EnvironmentTests(unittest.TestCase):
    def settings(self, appdata, **values):
        folder = Path(appdata) / 'bbport-launcher'
        folder.mkdir(parents=True)
        (folder / 'settings.json').write_text(json.dumps(values), encoding='utf-8')

    def test_nothing_when_play_online_is_off(self):
        with tempfile.TemporaryDirectory() as appdata:
            self.settings(appdata, online=False, online_npid='me')
            code, out, err = run('env', appdata=appdata)
            self.assertEqual((code, out.strip()), (0, ''), err)

    def test_shadnet_environment(self):
        with tempfile.TemporaryDirectory() as appdata:
            self.settings(appdata, online=True, online_pool='shadnet', online_server='play.example.org:31313',
                          online_npid='Hunter', online_password='p w', online_upnp=False,
                          online_community='https://example.org:443')
            code, out, err = run('env', appdata=appdata)
            self.assertEqual(code, 0, err)
            env = dict(line.split('=', 1) for line in out.splitlines())
            self.assertEqual(env['BB_ONLINE'], '1')
            self.assertEqual(env['BB_SHADNET_SERVER'], 'play.example.org:31313')
            self.assertEqual(env['BB_SHADNET_WEBAPI'], 'http://play.example.org:31315')
            self.assertEqual(env['BB_SHADNET_NPID'], 'Hunter')
            self.assertEqual(env['BB_SHADNET_PASSWORD'], 'p w')
            self.assertEqual(env['BB_UPNP'], '0')
            self.assertEqual(env['BB_COMMUNITY_SERVER'], 'https://example.org')
            self.assertNotIn('BB_BRIDGE', env)
            self.assertFalse(any(name.startswith('BB_GAME') for name in env))   # only the online variables

    def test_hunters_dream_starts_the_bridge_without_a_shadnet_account(self):
        with tempfile.TemporaryDirectory() as appdata:
            self.settings(appdata, online=True, online_pool='huntersdream', bridge_live_map=True,
                          online_npid='ignored')
            _code, out, _err = run('env', appdata=appdata)
            env = dict(line.split('=', 1) for line in out.splitlines())
            self.assertEqual(env['BB_BRIDGE'], '1')
            self.assertEqual(env['BB_BRIDGE_LIVE_MAP'], '1')
            self.assertEqual(env['BB_SHADNET_NPID'], '')

    def test_the_supermedo_pool_of_the_old_launcher_is_read_as_shadnet(self):
        with tempfile.TemporaryDirectory() as appdata:
            self.settings(appdata, online=True, online_pool='supermedo', supermedo_user='x',
                          online_server='play.example.org:31313')
            _code, out, _err = run('env', appdata=appdata)
            env = dict(line.split('=', 1) for line in out.splitlines())
            self.assertEqual(env['BB_SHADNET_SERVER'], 'play.example.org:31313')
            self.assertNotIn('BB_POOL_NAME', env)


class BridgeTests(unittest.TestCase):
    def test_makes_the_config_and_lists_the_accounts(self):
        with tempfile.TemporaryDirectory() as saves:
            folder = Path(saves) / 'bbbridge'
            folder.mkdir()
            (folder / 'bbbridge_state.json').write_text(json.dumps({'accounts': {'hunter-1': {}, 'hunter-2': {}}}))
            code, out, err = run('bridge', saves)
            self.assertEqual(code, 0, err)
            rows = [line.split('\t') for line in out.splitlines()]
            self.assertEqual([r[0] for r in rows if r[0] == 'account'], ['account', 'account'])
            self.assertEqual({r[1] for r in rows if r[0] == 'account'}, {'hunter-1', 'hunter-2'})
            self.assertEqual(Path([r[1] for r in rows if r[0] == 'folder'][0]), folder)
            config = Path([r[1] for r in rows if r[0] == 'config'][0])
            self.assertEqual(json.loads(config.read_text())['state_file'], str(folder / 'bbbridge_state.json'))


if __name__ == '__main__':
    unittest.main()
