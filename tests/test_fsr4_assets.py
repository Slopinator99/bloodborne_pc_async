"""The FSR 4 v07 download list (scripts/fsr4_assets.py, shared by both launchers) covers every tier
the provider can ask for, matches tools/fetch_fsr4_assets.sh, and launcher_fsr4.py reports it."""
from paths import ROOT
import importlib.util
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('fsr4_launcher', ROOT / 'launcher/bbport_launcher_win.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)

MODELS = ('native', 'quality', 'balanced', 'performance', 'ultraperf', 'drs')


def provider_tier(width, height):
    """ffxFsr4V07SelectAssetTier of gpu/third_party/fsr-vulkan (src/ffx_vk_fsr4_v07_assets.c)."""
    if width > 3840 or height > 2160:
        return '4320'
    if width > 1920 or height > 1080:
        return '2160'
    return '1080'


def tier_files(model, tier):
    stem = f'fsr4_model_v07_i8_{model}_{tier}'
    return [f'{stem}_pre.spv', f'{stem}_post.spv'] + [f'{stem}_pass{n}.spv' for n in range(1, 13)]


class Fsr4AssetsTests(unittest.TestCase):
    def test_every_output_tier_is_downloaded(self):
        files = set(launcher.fsr4_files())
        for output in ('1280x720', '1920x1080', '2560x1080', '2560x1440', '3440x1440', '3840x2160',
                       '5120x1440', '5120x2160', '7680x4320'):
            width, height = map(int, output.split('x'))
            for model in MODELS:
                with self.subTest(output=output, model=model):
                    self.assertLessEqual(set(tier_files(model, provider_tier(width, height))), files)

    def test_list_has_no_duplicates(self):
        files = launcher.fsr4_files()
        self.assertEqual(len(files), len(set(files)))
        self.assertEqual(len(files), 3 + len(MODELS) * (3 + 14 * len(launcher.FSR4_TIERS)))

    def test_script_downloads_the_same_tiers(self):
        script = (ROOT / 'tools/fetch_fsr4_assets.sh').read_text(encoding='utf-8')
        tiers = re.search(r'\$\{BB_FSR4_TIERS:-([^}]*)\}', script).group(1).split()
        self.assertEqual(tuple(tiers), launcher.FSR4_TIERS)

    def test_install_without_4320_reports_only_4320_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / 'fsr4_shaders'
            folder.mkdir()
            for name in launcher.fsr4_files():
                if '_4320_' not in name:
                    (folder / name).write_bytes(b'\0')
            saved, launcher.PORT_DIR = launcher.PORT_DIR, Path(tmp)
            try:
                missing = launcher.fsr4_missing()
            finally:
                launcher.PORT_DIR = saved
        self.assertEqual(len(missing), 14 * len(MODELS))
        self.assertTrue(all('_4320_' in name for name in missing))


sys.path.insert(0, str(ROOT / 'scripts'))
import fsr4_assets  # noqa: E402

HELPER = ROOT / 'scripts' / 'launcher_fsr4.py'


def run_helper(port):
    done = subprocess.run([sys.executable, '-I', str(HELPER), str(port)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8')
    return done.returncode, [line.split('\t') for line in done.stdout.splitlines()], done.stderr


class AssetListTests(unittest.TestCase):
    def test_list_has_the_whole_v07_set(self):
        files = fsr4_assets.fsr4_files()
        self.assertEqual(len(files), 3 + 6 * (3 + 3 * 14))
        self.assertEqual(len(set(files)), len(files))
        self.assertIn('fsr4_model_v07_i8_quality_1080_pass12.spv', files)
        self.assertIn('fsr4_model_v07_i8_drs_2160_post.spv', files)

    def test_same_commit_and_files_as_the_shell_script(self):
        script = (ROOT / 'tools' / 'fetch_fsr4_assets.sh').read_text(encoding='utf-8')
        self.assertEqual(re.search(r'^commit=(\w+)', script, re.M).group(1), fsr4_assets.FSR4_COMMIT)
        self.assertIn(f'FireBurn/Q2RTX/$commit/baseq2/fsr4_shaders', script)
        models = re.search(r'for model in ([\w ]+); do', script).group(1).split()
        tiers = re.search(r'BB_FSR4_TIERS:-([\d ]+)\}', script).group(1).split()
        expected = ['LICENSE-FSR4-v07.txt', 'rcas.spv', 'spd_auto_exposure.spv']
        for model in models:
            expected += [f'fsr4_model_v07_i8_{model}_{kind}' for kind in
                         ('initializers.bin', 'pre_weights.bin', 'shader_manifest.json')]
            for tier in tiers:
                expected += [f'fsr4_model_v07_i8_{model}_{tier}_pre.spv', f'fsr4_model_v07_i8_{model}_{tier}_post.spv']
                expected += [f'fsr4_model_v07_i8_{model}_{tier}_pass{n}.spv' for n in range(1, 13)]
        self.assertEqual(fsr4_assets.fsr4_files(), expected)

    def test_missing_ignores_present_files_and_counts_empty_ones_as_missing(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            self.assertEqual(fsr4_assets.fsr4_missing(folder), fsr4_assets.fsr4_files())
            (folder / 'rcas.spv').write_bytes(b'x')
            (folder / 'spd_auto_exposure.spv').write_bytes(b'')   # a failed download
            missing = fsr4_assets.fsr4_missing(folder)
            self.assertNotIn('rcas.spv', missing)
            self.assertIn('spd_auto_exposure.spv', missing)


class HelperTests(unittest.TestCase):
    def test_prints_the_base_and_every_file_with_its_state(self):
        with tempfile.TemporaryDirectory() as directory:
            port = Path(directory)
            (port / 'fsr4_shaders').mkdir()
            (port / 'fsr4_shaders' / 'rcas.spv').write_bytes(b'data')
            code, rows, err = run_helper(port)
            self.assertEqual(code, 0, err)
            self.assertEqual(rows[0], ['base', fsr4_assets.FSR4_BASE])
            files = {row[1]: row[2] for row in rows[1:]}
            self.assertEqual(len(files), len(fsr4_assets.fsr4_files()))
            self.assertEqual(files['rcas.spv'], '1')
            self.assertEqual(files['spd_auto_exposure.spv'], '0')

    def test_a_missing_port_folder_means_everything_is_missing(self):
        code, rows, _err = run_helper(Path(tempfile.gettempdir()) / 'no-such-port-folder')
        self.assertEqual(code, 0)
        self.assertTrue(all(row[2] == '0' for row in rows[1:]))

    def test_usage_error(self):
        done = subprocess.run([sys.executable, '-I', str(HELPER)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
        self.assertEqual(done.returncode, 2)


if __name__ == '__main__':
    unittest.main()
