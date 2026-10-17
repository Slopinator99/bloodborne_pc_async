#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Keyboard and mouse bindings for bb-launcher's Controls tab: the key_<action> lines of bbport.ini.

    launcher_input.py load <bbport.ini>
    launcher_input.py save <bbport.ini> ACTION=KEY[,MOUSE] ...

The game reads one key and one mouse input per action (src/input_bindings.c): "key_interact=E",
"key_firearm=X, Mouse2", "key_gestures=none". A key is an SDL scancode name ("E", "Left Shift",
"Space"), a mouse input is Mouse1..Mouse5 (left, right, middle, the two side buttons), WheelUp or
WheelDown.

load prints tab-separated lines:
    action<TAB>name<TAB>label<TAB>default key<TAB>default mouse   every action, in the game's order
    bind<TAB>name<TAB>key<TAB>mouse                               the current binding (defaults
                                                                  when the file has no line for it)
save writes the given bindings: ACTION=KEY, ACTION=KEY,MOUSE or ACTION=,MOUSE (a part left empty
means unbound; both empty writes "none"). Existing key_ lines are rewritten where they stand,
missing ones are appended, every other line of the file is kept.

The action table is checked against src/input_bindings.c by tests/test_launcher_input.py."""
import re
import sys
from pathlib import Path

# (name, label, default key, default mouse): the game's table, in its order.
ACTIONS = [
    ('move_forward', 'Move Forward', 'W', ''),
    ('move_back', 'Move Back', 'S', ''),
    ('move_left', 'Move Left', 'A', ''),
    ('move_right', 'Move Right', 'D', ''),
    ('walk', 'Walk', 'Left Alt', ''),
    ('roll', 'Dodge / Sprint', 'Space', ''),
    ('lock_on', 'Lock On', 'Q', ''),
    ('attack', 'Attack', '', 'Mouse1'),
    ('strong_attack', 'Strong Attack', 'V', ''),
    ('strong_modifier', 'Strong Attack Modifier', 'Left Shift', ''),
    ('transform', 'Transform', 'C', 'Mouse3'),
    ('firearm', 'Firearm / Parry', 'X', 'Mouse2'),
    ('blood_vial', 'Blood Vial', 'R', ''),
    ('use_item', 'Use Item', 'F', ''),
    ('interact', 'Interact', 'E', ''),
    ('switch_item', 'Switch Item', 'Down', ''),
    ('switch_right', 'Switch Right Weapon', 'Right', ''),
    ('switch_left', 'Switch Left Weapon', 'Left', ''),
    ('dpad_up', 'D-pad Up', 'Up', ''),
    ('gestures', 'Gestures', 'G', ''),
    ('personal_effects', 'Personal Effects', 'T', ''),
    ('look_up', 'Look Up', 'I', ''),
    ('look_down', 'Look Down', 'K', ''),
    ('look_left', 'Look Left', 'J', ''),
    ('look_right', 'Look Right', 'L', ''),
    ('menu', 'Menu', 'Escape', ''),
    ('confirm', 'Menu Confirm', 'Return', ''),
    ('back', 'Menu Back', 'Backspace', ''),
    ('l3', 'Left Stick Press', 'Left Ctrl', ''),
]
NAMES = {name for name, _l, _k, _m in ACTIONS}
MOUSE_INPUTS = ('Mouse1', 'Mouse2', 'Mouse3', 'Mouse4', 'Mouse5', 'WheelUp', 'WheelDown')
_MOUSE_LOWER = {m.lower(): m for m in MOUSE_INPUTS}


def parse_value(value):
    """(key, mouse) of a key_ line's value; 'none' and empty parts mean unbound."""
    key = mouse = ''
    for part in value.split(','):
        part = part.strip()
        if not part or part.lower() == 'none':
            continue
        if part.lower() in _MOUSE_LOWER:
            mouse = _MOUSE_LOWER[part.lower()]
        else:
            key = part
    return key, mouse


def format_value(key, mouse):
    parts = [p for p in (key, mouse) if p]
    return ', '.join(parts) if parts else 'none'


def read_lines(path):
    try:
        return Path(path).read_text(encoding='utf-8').splitlines()
    except OSError:
        return []


def bindings(lines):
    """{name: (key, mouse)}: the defaults, then each key_ line (the last one for an action wins, as
    in the game)."""
    current = {name: (key, mouse) for name, _label, key, mouse in ACTIONS}
    for line in lines:
        text = line.strip()
        if text.startswith('#') or '=' not in text or not text.startswith('key_'):
            continue
        name, value = (part.strip() for part in text[4:].split('=', 1))
        if name in NAMES:
            current[name] = parse_value(value)
    return current


def load_lines(path):
    current = bindings(read_lines(path))
    out = [f'action\t{name}\t{label}\t{key}\t{mouse}' for name, label, key, mouse in ACTIONS]
    out += [f'bind\t{name}\t{current[name][0]}\t{current[name][1]}' for name, _l, _k, _m in ACTIONS]
    return out


def save(path, edits):
    """edits: {name: (key, mouse)}. Rewrites key_ lines in place, appends the missing ones."""
    for name, (key, mouse) in edits.items():
        if name not in NAMES:
            raise ValueError(f'not an action: {name}')
        if mouse and mouse.lower() not in _MOUSE_LOWER:
            raise ValueError(f'not a mouse input: {mouse}')
        if key and (',' in key or key.lower() in _MOUSE_LOWER or key.lower() == 'none' or
                    re.fullmatch(r'(?i)mouse\d+|wheel\w*', key)):
            raise ValueError(f'not a key name: {key}')
    lines = read_lines(path)
    pending = {name: format_value(key, _MOUSE_LOWER.get(mouse.lower(), '') if mouse else '')
               for name, (key, mouse) in edits.items()}
    out, written = [], set()
    for line in lines:
        text = line.strip()
        match = re.match(r'key_(\w+)\s*=', text)
        if match and match.group(1) in pending:
            name = match.group(1)
            if name not in written:
                out.append(f'key_{name}={pending[name]}')
                written.add(name)
            continue   # a repeated line of the same action goes: the last one would have won
        out.append(line)
    for name, value in pending.items():
        if name not in written:
            out.append(f'key_{name}={value}')
    Path(path).write_text('\n'.join(out) + '\n', encoding='utf-8')


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    if len(argv) == 3 and argv[1] == 'load':
        print('\n'.join(load_lines(argv[2])))
        return 0
    if len(argv) >= 3 and argv[1] == 'save':
        edits = {}
        for item in argv[3:]:
            if '=' not in item:
                print(f'unexpected argument: {item}', file=sys.stderr)
                return 2
            name, value = item.split('=', 1)
            edits[name] = parse_value(value)
        if not edits:
            return 0
        try:
            save(argv[2], edits)
        except (ValueError, OSError) as error:
            print(error, file=sys.stderr)
            return 1
        print('ok')
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
