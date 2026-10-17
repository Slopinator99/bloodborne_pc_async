"""scripts/launcher_coop.py: seamless co-op for the native launcher (the rules are bbport_coop's)."""
from paths import ROOT
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = ROOT / 'scripts' / 'launcher_coop.py'


def run(*args, appdata, local):
    env = dict(os.environ, APPDATA=str(appdata), LOCALAPPDATA=str(local))
    done = subprocess.run([sys.executable, '-I', str(SCRIPT), *args], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8', env=env, timeout=120)
    return done.returncode, done.stdout, done.stderr


@unittest.skipUnless(os.name == 'nt', 'co-op setup uses Windows tools')
class CoopHelperTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.appdata = Path(self.temporary.name) / 'appdata'
        self.local = Path(self.temporary.name) / 'local'
        (self.appdata / 'bbport-launcher').mkdir(parents=True)
        self.local.mkdir()

    def settings(self, **values):
        (self.appdata / 'bbport-launcher' / 'settings.json').write_text(json.dumps(values), encoding='utf-8')

    def run_helper(self, *args):
        return run(*args, appdata=self.appdata, local=self.local)

    def test_status_of_a_fresh_install(self):
        code, out, err = self.run_helper('status')
        self.assertEqual(code, 0, err)
        rows = dict(line.split('\t')[:2] for line in out.splitlines())
        self.assertEqual((rows['client'], rows['server'], rows['setup'], rows['hosting']), ('0', '0', '0', '0'))

    def test_env_needs_a_name_and_password(self):
        code, out, _err = self.run_helper('env')
        self.assertEqual(code, 1)
        self.assertTrue(out.startswith('error\t'), out)

    def test_env_needs_a_host_or_an_invite(self):
        self.settings(coop_name='Hunter', coop_password='secret')
        code, out, _err = self.run_helper('env')
        self.assertEqual(code, 1)
        self.assertIn('An invite code starts with', out)
        self.assertIn('host a session yourself', out)

    def test_env_needs_the_setup(self):
        sys.path.insert(0, str(ROOT / 'launcher'))
        import bbport_coop
        self.settings(coop_name='Hunter', coop_password='secret',
                      coop_invite=bbport_coop.make_invite('100.64.0.1', 'key'))
        code, out, _err = self.run_helper('env')
        self.assertEqual(code, 1)
        self.assertIn('Set up co-op first', out)

    def test_a_damaged_invite_is_refused(self):
        self.settings(coop_name='Hunter', coop_password='secret', coop_invite='BBCOOP-!!')
        code, out, _err = self.run_helper('env')
        self.assertEqual(code, 1)
        self.assertIn('damaged', out)

    def test_host_needs_a_name_and_password(self):
        code, out, _err = self.run_helper('host')
        self.assertEqual(code, 1)
        self.assertIn('co-op name and password', out)

    def test_usage_error(self):
        self.assertEqual(self.run_helper()[0], 2)
        self.assertEqual(self.run_helper('status', 'extra')[0], 2)


if __name__ == '__main__':
    unittest.main()
