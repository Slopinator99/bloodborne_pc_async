"""The log encoding contract: UTF-8 streams and children, whatever the ANSI codepage."""
from paths import ROOT
import os
from pathlib import Path
import sys
import tempfile
import unittest

if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
import run  # noqa: E402  (needs ROOT on sys.path first)


class LogEncodingTests(unittest.TestCase):
    def setUp(self):
        self.saved_port = run.PORT
        self.saved_io = os.environ.pop('PYTHONIOENCODING', None)

    def tearDown(self):
        run.PORT = self.saved_port
        if self.saved_io is not None:
            os.environ['PYTHONIOENCODING'] = self.saved_io
        else:
            os.environ.pop('PYTHONIOENCODING', None)

    def child(self, source):
        """Point run.PORT at a temp port tree holding one script."""
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        scripts = Path(temp.name) / 'scripts'
        scripts.mkdir()
        (scripts / 'enc_child.py').write_text(source, encoding='utf-8')
        run.PORT = Path(temp.name)

    def test_force_utf8_sets_env_for_children(self):
        run.force_utf8()
        self.assertEqual(os.environ.get('PYTHONIOENCODING'), 'utf-8')

    def test_capture_decodes_child_output_as_utf8(self):
        # Without the fix the child inherits the ANSI codepage (cp1252), emits 0x99 for
        # "TM", and run_script's decode turns it into U+FFFD (or raises on strict codecs).
        self.child("import sys\nprint(sys.argv[1])\n")
        run.force_utf8()
        result = run.run_script('enc_child.py', '\u2122\u00e9\u4e16', capture=True)
        self.assertEqual(result.strip(), '\u2122\u00e9\u4e16')

    def test_stray_non_utf8_bytes_replace_not_crash(self):
        # A native (non-Python) child can still put arbitrary bytes on the pipe.
        self.child("import sys\nsys.stdout.buffer.write(b'\\x99\\n')\n")
        run.force_utf8()
        result = run.run_script('enc_child.py', capture=True)
        self.assertEqual(result, '\ufffd\n')


if __name__ == '__main__':
    unittest.main()
