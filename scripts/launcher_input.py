#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""input.ini for bb-launcher's Controls tab (keyboard and mouse-button bindings and mouse look).

    launcher_input.py load <input.ini>
    launcher_input.py save <input.ini> [output=name ...] [output= ...] [--mouse on|off,stick,speed,offset]

load prints tab-separated lines:
    action<TAB>output<TAB>label<TAB>buttons|sticks      every editable action, in display order
    bind<TAB>output<TAB>first binding (empty when there is none)
    mouse<TAB>1|0<TAB>left|right<TAB>speed<TAB>speed offset (<TAB>smoothness, not edited)
    hotkeys<TAB>toggle key<TAB>reload key
    reserved<TAB>names that cannot be bound, comma separated
    keys<TAB>every key name the game accepts, comma separated

save writes the given first bindings (an empty name clears one) and, with --mouse, the mouse
look settings, keeping every other line of the file as the Python launcher's save does.

All of it is launcher/bbport_input_config.py, shared with the Python launcher."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
import bbport_input_config as cfg  # noqa: E402


def load_lines(path):
    bindings, _lines, toggle, reload_key, mouse = cfg.load_input_ini(path)
    out = []
    for group, outputs in (('buttons', cfg.BUTTON_OUTPUTS), ('sticks', cfg.STICK_OUTPUTS)):
        for output in outputs:
            out.append(f'action\t{output}\t{cfg.OUTPUT_LABELS[output][0]}\t{group}')
    for output in cfg.KEYBOARD_OUTPUTS:
        values = bindings[output]
        out.append(f'bind\t{output}\t{values[0] if values else ""}')
    out.append('mouse\t{}\t{}\t{:g}\t{:g}\t{:g}'.format(1 if mouse['enabled'] else 0, mouse['stick'], mouse['speed'],
                                                        mouse['speed_offset'], mouse['deadzone_offset']))
    out.append(f'hotkeys\t{toggle}\t{reload_key}')
    out.append('reserved\t' + ','.join(sorted(cfg.reserved_keys(toggle, reload_key))))
    out.append('keys\t' + ','.join([*cfg.KEY_NAMES, *cfg.MOUSE_BUTTON_NAMES]))
    return out


def save(path, edits, mouse_arg):
    bindings, lines, _toggle, _reload, mouse = cfg.load_input_ini(path)
    first = {output: (values[0] if values else None) for output, values in bindings.items()}
    for output, name in edits.items():
        if output not in first:
            raise ValueError(f'not an editable action: {output}')
        if name and name not in cfg.BINDABLE_NAMES_SET:
            raise ValueError(f'not a key or button name: {name}')
        if name and output in cfg.STICK_OUTPUTS and name not in cfg.KEY_NAMES_SET:
            raise ValueError(f'a stick direction takes a key only: {output}={name}')
        first[output] = name or None
    if edits:
        cfg.save_input_ini(path, first, lines)
    if mouse_arg:
        state, stick, speed, offset = mouse_arg.split(',')
        if stick not in ('left', 'right'):
            raise ValueError(f'mouse stick must be left or right: {stick}')
        mouse = dict(mouse, enabled=state in ('1', 'on'), stick=stick, speed=round(float(speed), 3),
                     speed_offset=round(float(offset), 3))
        _b, lines, _t, _r, _m = cfg.load_input_ini(path)
        cfg.save_mouse_settings(path, mouse, lines)


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8')
        except (AttributeError, ValueError):
            pass
    if len(argv) >= 3 and argv[1] == 'load':
        print('\n'.join(load_lines(argv[2])))
        return 0
    if len(argv) >= 3 and argv[1] == 'save':
        edits, mouse_arg, rest = {}, None, argv[3:]
        while rest:
            item = rest.pop(0)
            if item == '--mouse' and rest:
                mouse_arg = rest.pop(0)
            elif '=' in item:
                output, name = item.split('=', 1)
                edits[output] = name
            else:
                print(f'unexpected argument: {item}', file=sys.stderr)
                return 2
        try:
            save(argv[2], edits, mouse_arg)
        except (ValueError, OSError) as error:
            print(error, file=sys.stderr)
            return 1
        print('ok')
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
