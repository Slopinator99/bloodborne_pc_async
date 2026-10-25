#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""What each thread of a Windows minidump (the hang and crash dumps of the port, docs/LOGGING.md)
was doing, without a debugger: the thread names, the instruction pointer and the return addresses
found on each stack, with function names when the unstripped bb-probe.exe is given.

    minidump_threads.py DUMP [UNSTRIPPED_BB_PROBE.exe]

The exe must be the build the dump came from (a package's bb-probe.exe is that build with its
debug information removed: build with build.sh and use out/bb-probe.exe of the same commit).
Stack scanning is a heuristic (there is no unwind information): the addresses are candidates in
stack order, nearest first, and some are stale leftovers."""
import struct
import subprocess
import sys
from pathlib import Path

THREAD_LIST, MODULE_LIST, THREAD_NAMES = 3, 4, 24
CONTEXT_RSP, CONTEXT_RIP = 0x98, 0xF8


def read_string(data, rva):
    (length,) = struct.unpack_from('<I', data, rva)
    return data[rva + 4:rva + 4 + length].decode('utf-16-le', errors='replace')


def parse(data):
    if data[:4] != b'MDMP':
        raise ValueError('not a minidump')
    count, directory = struct.unpack_from('<II', data, 8)
    streams = {}
    for i in range(count):
        kind, size, rva = struct.unpack_from('<III', data, directory + 12 * i)
        streams[kind] = (size, rva)
    modules = []
    if MODULE_LIST in streams:
        _, rva = streams[MODULE_LIST]
        (n,) = struct.unpack_from('<I', data, rva)
        for i in range(n):
            base, size = struct.unpack_from('<QI', data, rva + 4 + 108 * i)
            (name_rva,) = struct.unpack_from('<I', data, rva + 4 + 108 * i + 20)
            modules.append((base, size, read_string(data, name_rva)))
    names = {}
    if THREAD_NAMES in streams:
        _, rva = streams[THREAD_NAMES]
        (n,) = struct.unpack_from('<I', data, rva)
        for i in range(n):
            tid, name_rva = struct.unpack_from("<IQ", data, rva + 4 + 12 * i)
            names[tid] = read_string(data, name_rva)
    threads = []
    _, rva = streams[THREAD_LIST]
    (n,) = struct.unpack_from('<I', data, rva)
    for i in range(n):
        entry = rva + 4 + 48 * i
        tid, _susp, _prio_class, _prio, _teb = struct.unpack_from('<IIIIQ', data, entry)
        stack_start, stack_size, stack_rva = struct.unpack_from('<QII', data, entry + 24)
        ctx_size, ctx_rva = struct.unpack_from('<II', data, entry + 40)
        rip = struct.unpack_from('<Q', data, ctx_rva + CONTEXT_RIP)[0]
        rsp = struct.unpack_from('<Q', data, ctx_rva + CONTEXT_RSP)[0]
        threads.append({'tid': tid, 'name': names.get(tid, ''), 'rip': rip, 'rsp': rsp,
                        'stack': (stack_start, data[stack_rva:stack_rva + stack_size])})
    return modules, threads


def module_for(modules, address):
    for base, size, name in modules:
        if base <= address < base + size:
            return base, name
    return None


def stack_candidates(thread, modules, wanted, limit=14):
    """Addresses on the stack that point into the modules whose name ends with `wanted`."""
    start, memory = thread['stack']
    found = []
    offset = max(0, thread['rsp'] - start)
    for position in range(offset - offset % 8, len(memory) - 7, 8):
        (value,) = struct.unpack_from('<Q', memory, position)
        hit = module_for(modules, value)
        if hit and hit[1].lower().endswith(wanted):
            found.append(value)
            if len(found) >= limit:
                break
    return found


def symbolize(exe, offsets):
    """Function names for module-relative offsets, from llvm-symbolizer (empty strings without)."""
    if not exe or not offsets:
        return [''] * len(offsets)
    try:
        done = subprocess.run(['llvm-symbolizer', '--obj=' + str(exe), '--relative-address', '--demangle',
                               '--inlining=false', '--functions=short', '--output-style=GNU'],
                              input='\n'.join(f'0x{o:x}' for o in offsets) + '\n', capture_output=True, text=True,
                              timeout=300)
    except (OSError, subprocess.SubprocessError):
        return [''] * len(offsets)
    lines = done.stdout.split('\n')
    # GNU style: function, file:line per address, then a blank line
    out, i = [], 0
    for _ in offsets:
        function = lines[i] if i < len(lines) else ''
        location = lines[i + 1] if i + 1 < len(lines) else ''
        out.append(f'{function} ({Path(location).name})' if function and function != '??' else '')
        i += 3
    return out


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2
    data = Path(argv[1]).read_bytes()
    exe = argv[2] if len(argv) == 3 else None
    modules, threads = parse(data)
    probe = next(((base, size) for base, size, name in modules if name.lower().endswith('bb-probe.exe')), None)
    print(f'{len(threads)} threads; bb-probe.exe at {hex(probe[0]) if probe else "?"}')
    for thread in threads:
        where = module_for(modules, thread['rip'])
        name = where[1].rsplit('\\', 1)[-1] if where else '?'
        line = f'thread {thread["tid"]:6d} {thread["name"] or "-":28s} rip {name}+0x{thread["rip"] - where[0]:x}' if where \
            else f'thread {thread["tid"]:6d} {thread["name"] or "-":28s} rip {hex(thread["rip"])}'
        print(line)
        if probe:
            candidates = stack_candidates(thread, modules, 'bb-probe.exe')
            names = symbolize(exe, [c - probe[0] for c in candidates])
            for value, function in zip(candidates, names):
                print(f'      bb-probe.exe+0x{value - probe[0]:x}  {function}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
