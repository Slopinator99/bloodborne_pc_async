#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The standard-library modules the port's scripts import, one per line, for package.sh.

The scripts run inside bb-runtime.exe (run.py, scripts/*.py through --script). PyInstaller only
bundles what the frozen entry point's own imports reach, so a script's `import platform` fails
inside the package with a module that is missing: a game that does not start. package.sh gives
every module printed here to PyInstaller as a hidden import.

    script_imports.py PATH...     files, or folders searched for *.py
"""
import ast
from pathlib import Path
import sys


def imported_modules(source):
    """The top-level names imported anywhere in `source` (a string of Python)."""
    names = set()
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Import):
            names.update(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            names.add(node.module)
    return names


def standard_library_imports(paths):
    found = set()
    for path in map(Path, paths):
        files = sorted(path.glob('*.py')) if path.is_dir() else [path]
        for file in files:
            try:
                found |= imported_modules(file.read_text(encoding='utf-8'))
            except (OSError, SyntaxError, UnicodeDecodeError):
                continue
    return sorted(name for name in found if name.split('.')[0] in sys.stdlib_module_names)


if __name__ == '__main__':
    print('\n'.join(standard_library_imports(sys.argv[1:])))
