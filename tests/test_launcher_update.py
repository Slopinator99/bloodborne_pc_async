"""bb-update.h (the C launcher's updater) against a local fake GitHub release server."""
from paths import ROOT
import http.server
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
import zipfile

MSYS_CLANG = Path(os.environ.get('MSYS2_ROOT', 'C:/msys64')) / 'clang64' / 'bin'


def find_clang():
    return shutil.which('clang') or (str(MSYS_CLANG / 'clang.exe') if (MSYS_CLANG / 'clang.exe').is_file() else None)


def package_zip():
    """A release package: the launcher at its root folder, data, and a user\\ it must not install."""
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as package:
        package.writestr('bbport-windows/bb-launcher.exe', 'NEW LAUNCHER ' * 2000)
        package.writestr('bbport-windows/data/new.txt', 'fresh')
        package.writestr('bbport-windows/scripts/deep/er.py', 'nested')
        package.writestr('bbport-windows/user/save.dat', 'PACKAGE SAVE (must not be installed)')
        package.writestr('bbport-windows/bbport.ini', 'package settings (must not be installed)')
        package.writestr('bbport-windows/mods.json', 'package mod order (must not be installed)')
    return out.getvalue()


class Release(http.server.BaseHTTPRequestHandler):
    zip_bytes = package_zip()
    port = 0

    def do_GET(self):
        if self.path == '/releases/latest':
            body = json.dumps({
                'html_url': f'http://127.0.0.1:{self.port}/page', 'tag_name': 'windows-v1.8.0',
                'assets': [{'name': 'x.7z', 'browser_download_url': f'http://127.0.0.1:{self.port}/x.7z'},
                           {'name': 'package.zip', 'browser_download_url': f'http://127.0.0.1:{self.port}/package.zip'}],
            }).encode()
            kind = 'application/json'
        elif self.path == '/package.zip':
            body, kind = self.zip_bytes, 'application/zip'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


@unittest.skipUnless(os.name == 'nt', 'bb-update.h uses WinHTTP')
class UpdaterTests(unittest.TestCase):
    def test_check_download_unpack_and_install_keep_user_files(self):
        clang = find_clang()
        if not clang:
            self.skipTest('clang not found')
        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Release)
        Release.port = server.server_address[1]
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                install = work / 'install'
                (install / 'user').mkdir(parents=True)
                (install / 'user' / 'save.dat').write_text('MY SAVE')
                (install / 'mods' / 'mine').mkdir(parents=True)
                (install / 'mods' / 'mine' / 'file.txt').write_text('my mod')
                (install / 'bbport.ini').write_text('my settings')
                (install / 'mods.json').write_text('my mod order')
                (install / 'bb-launcher.exe').write_text('OLD LAUNCHER')
                exe = work / 'test_launcher_update.exe'
                build = subprocess.run([clang, '-std=c11', '-Wall', '-Wextra', '-Wno-unused-function', '-o', str(exe),
                                        str(ROOT / 'tests' / 'test_launcher_update.c'), '-lwinhttp'],
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                self.assertEqual(build.returncode, 0, build.stdout)
                self.assertNotIn('warning', build.stdout)
                run = subprocess.run([str(exe), str(Release.port), str(work)], stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, timeout=120)
                self.assertEqual(run.returncode, 0, run.stdout)
                self.assertIn('OK', run.stdout)
                # What the C test checked, seen from here too.
                self.assertEqual((install / 'user' / 'save.dat').read_text(), 'MY SAVE')
                self.assertEqual((install / 'bbport.ini').read_text(), 'my settings')
                self.assertTrue((install / 'bb-launcher.exe').read_text().startswith('NEW LAUNCHER'))
        finally:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    unittest.main()
