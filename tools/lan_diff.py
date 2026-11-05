"""Compare the two saves of a LAN bug report (OUT OF STEP).

When the games of a LAN game go apart, both sides save the game at the same
step (LAN_OOS_<step>_M.lan.save on the server, LAN_OOS_<step>_S.lan.save on
the player who joined, which also goes to the server) and the commands of
the last turns (LAN_OOS_<step>_M.txt / _S.txt). This prints what differs:

  python tools/lan_diff.py LAN_OOS_5300_M.lan.save LAN_OOS_5300_S.lan.save
      [--max 40]   at most this many differences per kind (default 40)

The saves are Freeserf text saves: sections "[kind index]" with lines
"  key = value". The map is kept in blocks of 16x16 tiles ("[map n]",
pos = column,row of the block, then one value per tile); a different tile
is shown with its column and row. Then the two command logs, if they lie
next to the saves, from the first line where they differ.
"""
import argparse
import os
import sys
from collections import OrderedDict

BLOCK = 16


def read_save(path):
    """{section: OrderedDict(key: value)} in the order of the file."""
    sections = OrderedDict()
    current = None
    with open(path, encoding='latin-1') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.startswith('[') and line.endswith(']'):
                current = OrderedDict()
                sections[line[1:-1]] = current
            elif current is not None and '=' in line:
                key, value = line.split('=', 1)
                current[key.strip()] = value.strip()
    return sections


def kind_of(section):
    return section.split(' ', 1)[0]


def diff_map(name, a, b, out):
    """The tiles of a map block that differ: (key, col, row, a, b)."""
    pos = a.get('pos', b.get('pos', '0,0')).split(',')
    bx, by = int(pos[0]), int(pos[1])
    for key in sorted(set(a) | set(b)):
        if key == 'pos':
            continue
        va = a.get(key, '').split(',')
        vb = b.get(key, '').split(',')
        for i in range(max(len(va), len(vb))):
            xa = va[i] if i < len(va) else '-'
            xb = vb[i] if i < len(vb) else '-'
            if xa != xb:
                out.append((key, bx + i % BLOCK, by + i // BLOCK, xa, xb))


def compare(path_a, path_b, most):
    a = read_save(path_a)
    b = read_save(path_b)
    by_kind = OrderedDict()
    tiles = []
    for name in list(a) + [n for n in b if n not in a]:
        kind = kind_of(name)
        if name not in a or name not in b:
            by_kind.setdefault(kind, []).append(
                '%s: only in %s' % (name, 'A' if name in a else 'B'))
            continue
        sa, sb = a[name], b[name]
        if kind == 'map':
            diff_map(name, sa, sb, tiles)
            continue
        for key in list(sa) + [k for k in sb if k not in sa]:
            va, vb = sa.get(key, '(none)'), sb.get(key, '(none)')
            if va != vb:
                by_kind.setdefault(kind, []).append(
                    '[%s] %s: A %s | B %s' % (name, key, va[:100], vb[:100]))

    print('A: %s' % path_a)
    print('B: %s' % path_b)
    if not by_kind and not tiles:
        print('The two saves are the same.')
        return 0
    for kind, lines in by_kind.items():
        print('\n%s: %d difference(s)' % (kind, len(lines)))
        for line in lines[:most]:
            print('  ' + line)
        if len(lines) > most:
            print('  ... %d more' % (len(lines) - most))
    if tiles:
        print('\nmap: %d tile value(s) differ' % len(tiles))
        for key, col, row, xa, xb in tiles[:most]:
            print('  %s at %d,%d: A %s | B %s' % (key, col, row, xa, xb))
        if len(tiles) > most:
            print('  ... %d more' % (len(tiles) - most))
    return 1


def log_of(save_path):
    """LAN_OOS_<step>_M.lan.save -> LAN_OOS_<step>_M.txt"""
    base = save_path
    for end in ('.lan.save', '.save'):
        if base.endswith(end):
            base = base[:-len(end)]
            break
    return base + '.txt'


def compare_logs(path_a, path_b):
    la, lb = log_of(path_a), log_of(path_b)
    if not (os.path.exists(la) and os.path.exists(lb)):
        return
    with open(la, encoding='latin-1') as f:
        ta = f.read().split('\ncommands')
    with open(lb, encoding='latin-1') as f:
        tb = f.read().split('\ncommands')
    print('\n--- %s' % la)
    print(ta[0].strip())
    print('--- %s' % lb)
    print(tb[0].strip())
    if len(ta) < 2 or len(tb) < 2:
        return
    ca = ta[1].split('\n')[1:]
    cb = tb[1].split('\n')[1:]
    # The logs may start at other turns: line them up on the first turn
    # both have.
    sa = set(ca)
    common = [line for line in cb if line in sa]
    if common:
        ca = ca[ca.index(common[0]):]
        cb = cb[cb.index(common[0]):]
    for i in range(max(len(ca), len(cb))):
        xa = ca[i] if i < len(ca) else '(end)'
        xb = cb[i] if i < len(cb) else '(end)'
        if xa != xb:
            print('\nThe commands differ from line %d:' % (i + 1))
            for j in range(i, min(i + 10, max(len(ca), len(cb)))):
                print('  A %s' % (ca[j] if j < len(ca) else '(end)'))
                print('  B %s' % (cb[j] if j < len(cb) else '(end)'))
            return
    print('\nThe commands of both sides are the same (%d lines).' % len(ca))


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('save_a')
    p.add_argument('save_b')
    p.add_argument('--max', type=int, default=40)
    args = p.parse_args()
    r = compare(args.save_a, args.save_b, args.max)
    compare_logs(args.save_a, args.save_b)
    return r


if __name__ == '__main__':
    sys.exit(main())
