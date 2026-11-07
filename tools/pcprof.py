#!/usr/bin/env python3
"""Functions of the samples of PCSAMPLE (port/amiga_sampler.c).

usage: python tools/pcprof.py Settlers-pcs.bin [binary] [-n 40] [-f NAME]

-f NAME: the disassembly of the function NAME begins with, with the
samples of each instruction.

The symbols come from m68k-amigaos-nm in the build image (docker); the
address of amiga_sampler_start in the file gives the start of the code.
"""
import bisect
import collections
import os
import struct
import subprocess
import sys

IMAGE = 'sacredbanana/amiga-compiler:m68k-amigaos'


def symbols(binary):
    d = os.path.dirname(os.path.abspath(binary))
    b = os.path.basename(binary)
    out = subprocess.run(
        ['docker', 'run', '--rm', '-v', d + ':/w', IMAGE, 'sh', '-c',
         'm68k-amigaos-nm -n -C /w/' + b], capture_output=True, text=True,
        check=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[1] in 'tTwW':
            syms.append((int(parts[0], 16), parts[2]))
    syms.sort()
    return syms


def main():
    args = [a for a in sys.argv[1:]]
    top = 40
    if '-n' in args:
        i = args.index('-n')
        top = int(args[i + 1])
        del args[i:i + 2]
    func = None
    if '-f' in args:
        i = args.index('-f')
        func = args[i + 1]
        del args[i:i + 2]
    pcs = args[0]
    binary = args[1] if len(args) > 1 else 'build/amiga/Settlers'
    data = open(pcs, 'rb').read()
    magic, marker, n = struct.unpack('>4sII', data[:12])
    assert magic == b'PCS1', magic
    samples = struct.unpack('>%dI' % n, data[12:12 + 4 * n])
    syms = symbols(binary)
    addrs = [a for a, _ in syms]
    names = [s for _, s in syms]
    off = [a for a, s in syms if s.startswith('amiga_sampler_start')]
    base = marker - off[0]
    end = addrs[-1] + 0x10000
    count = collections.Counter()
    idle = outside = 0
    for pc in samples:
        if pc == 0:
            idle += 1
            continue
        if pc == 1:
            outside += 1
            continue
        rel = pc - base
        if rel < 0 or rel > end:
            outside += 1
            continue
        i = bisect.bisect_right(addrs, rel) - 1
        count[names[i] if i >= 0 else '?'] += 1
    if func is not None:
        k = [j for j, s in enumerate(names) if s.startswith(func)][0]
        lo, hi = addrs[k], addrs[k + 1]
        hits = collections.Counter(pc - base for pc in samples
                                   if lo <= pc - base < hi)
        d = os.path.dirname(os.path.abspath(binary))
        dis = subprocess.run(
            ['docker', 'run', '--rm', '-v', d + ':/w', IMAGE, 'sh', '-c',
             'm68k-amigaos-objdump -d -C --start-address=%d --stop-address=%d'
             ' /w/%s' % (lo, hi, os.path.basename(binary))],
            capture_output=True, text=True).stdout
        for line in dis.splitlines():
            m = line.strip().split(':', 1)
            try:
                a = int(m[0], 16)
            except ValueError:
                continue
            print('%5s %s' % (hits.get(a, '') or '', line[:100]))
        return
    busy = n - idle
    print('samples %d, game waiting %d (%.1f%%), outside the program '
          '(system, libraries) %d (%.1f%% of the busy ones)'
          % (n, idle, 100.0 * idle / max(1, n), outside,
             100.0 * outside / max(1, busy)))
    for name, c in count.most_common(top):
        print('%6.2f%%  %6d  %s' % (100.0 * c / max(1, busy), c, name[:110]))


if __name__ == '__main__':
    main()
