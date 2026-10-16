# SPDX-License-Identifier: GPL-2.0-or-later
"""The FSR 4 (v07, INT8) shader assets the port downloads: which files, and from where.

Shared by the Python launcher, the native launcher (through launcher_fsr4.py) and checked against
tools/fetch_fsr4_assets.sh by the tests. The files are built from AMD's MIT-licensed FidelityFX
source and published by FireBurn/Q2RTX; the commit is pinned."""
import os
from pathlib import Path

# Output tiers of the v07 model: up to 1920x1080, up to 3840x2160, and wider or taller outputs
# (5120x1440, 7680x4320). Each tier's SPIR-V bakes its tensor pitch, so no tier stands in for another.
FSR4_TIERS = ('1080', '2160', '4320')
FSR4_COMMIT = 'ae8d628fae208813172446d1e49ed94150b04658'
# BB_FSR4_BASE: another source of the same files (a mirror, or a test server).
FSR4_BASE = os.environ.get('BB_FSR4_BASE') or (
    f'https://raw.githubusercontent.com/FireBurn/Q2RTX/{FSR4_COMMIT}/baseq2/fsr4_shaders')


def fsr4_files():
    """The FSR 4 v07 asset set of tools/fetch_fsr4_assets.sh (every tier in FSR4_TIERS)."""
    files = ['LICENSE-FSR4-v07.txt', 'rcas.spv', 'spd_auto_exposure.spv']
    for model in ('native', 'quality', 'balanced', 'performance', 'ultraperf', 'drs'):
        files += [f'fsr4_model_v07_i8_{model}_initializers.bin', f'fsr4_model_v07_i8_{model}_pre_weights.bin',
                  f'fsr4_model_v07_i8_{model}_shader_manifest.json']
        for tier in FSR4_TIERS:
            files += [f'fsr4_model_v07_i8_{model}_{tier}_pre.spv', f'fsr4_model_v07_i8_{model}_{tier}_post.spv']
            files += [f'fsr4_model_v07_i8_{model}_{tier}_pass{n}.spv' for n in range(1, 13)]
    return files


def fsr4_missing(folder):
    """The files of fsr4_files() that are absent or empty in folder."""
    folder = Path(folder)
    return [name for name in fsr4_files() if not (folder / name).is_file() or not (folder / name).stat().st_size]
