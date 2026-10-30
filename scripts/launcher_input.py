#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The key_<action> bindings of bbport.ini for bb-launcher's Controls tab. The action table, the
default bindings and the reading and writing of the lines are launcher/bbport_bindings.py's (also
the Python launcher's), so the launchers and the engine (src/input_bindings.c) cannot disagree.

    launcher_input.py load <bbport.ini>
        "action<TAB>name<TAB>label" for each action, then "bind<TAB>name<TAB>b1<TAB>b2<TAB>b3": up
        to three bindings, each as the game writes it ("E", "Left Shift+Mouse1"), empty when unused
    launcher_input.py save <bbport.ini> ACTION=BINDING[,BINDING[,BINDING]] ...
        writes the lines that differ from the defaults, removes the others, keeps every other
        line of the file. "none" or an empty value unbinds the action.
    launcher_input.py reset <bbport.ini>
        removes every key_ line: the game's defaults apply again

A binding is one input or up to three joined with "+" and held together; commas separate
bindings. An action can have three."""
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
import bbport_bindings as bindings  # noqa: E402

KEY_PREFIX = 'key_'


def load_lines(ini):
    """The tab-separated protocol lines for `ini`."""
    values = bindings.read(ini)
    lines = []
    for action, label, _default in bindings.ACTIONS:
        lines.append(f'action\t{action}\t{label}')
    for action, _label, _default in bindings.ACTIONS:
        slots = values[action]
        cells = [bindings.format_value([slots[i]]) if i < len(slots) else '' for i in range(bindings.SLOTS)]
        lines.append('bind\t' + action + '\t' + '\t'.join(cell if cell != 'none' else '' for cell in cells))
    return lines


def parse_assignments(arguments):
    """{action: slots} from ACTION=VALUE arguments; ValueError for an unknown action or input."""
    result = {}
    for argument in arguments:
        action, _eq, value = argument.partition('=')
        if action not in bindings.ACTION_LABELS:
            raise ValueError(f'unknown action {action!r}')
        slots = bindings.parse_value(value)
        for slot in slots:
            for name in slot:
                if name.lower().startswith(('mouse', 'wheel')) and name not in bindings.MOUSE_NAMES:
                    raise ValueError(f'unknown mouse input {name!r}')
        result[action] = slots
    return result


def save(ini, assignments):
    """Rewrites the key_ lines of `assignments` in `ini`: a line only where it differs from the
    default, no line where it equals it. Every other line stays as it is."""
    defaults = bindings.defaults()
    path = Path(ini)
    try:
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
    except OSError:
        lines = []
    lines = [line for line in lines
             if not (line.strip().startswith(KEY_PREFIX) and line.strip()[len(KEY_PREFIX):].split('=')[0].strip() in assignments)]
    for action, slots in assignments.items():
        if slots != defaults[action]:
            lines.append(f'{KEY_PREFIX}{action}={bindings.format_value(slots)}')
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='')


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    command = argv[1] if len(argv) > 1 else ''
    try:
        if command == 'load' and len(argv) == 3:
            print('\n'.join(load_lines(argv[2])))
            return 0
        if command == 'save' and len(argv) >= 3:
            save(argv[2], parse_assignments(argv[3:]))
            return 0
        if command == 'reset' and len(argv) == 3:
            bindings.remove_all(argv[2])
            return 0
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
