"""Collect the files the intro player needs into one directory (the game's data/intro).

  python make_intro_dir.py [DEST]          (default build/intro/files)
Reads the names from build/intro/intro.script (LOAD records and module pointers), copies
them from data/disk1, then data/disk2 (the player's own disks, never into the repository),
and copies intro.script itself. Prints the list of files.
"""
import os
import shutil
import struct
import sys
from introlib import DATA, DATA2, OUT


def needed(script):
    d = script
    assert d[:4] == b"STIS"
    nb = struct.unpack(">H", d[6:8])[0]
    p = 12 + 4 * nb
    names = []

    def ptr():
        nonlocal p
        b = struct.unpack(">H", d[p:p + 2])[0]
        if b == 0xFFFF:
            n = d[p + 2:p + 6].decode("latin-1")
            if n not in names:
                names.append(n)
            p += 10
        else:
            p += 6
    while p < len(d):
        c = d[p]
        p += 1
        if c == 0x01:
            p += 2
        elif c in (0x02, 0x03, 0x1A, 0x1B):
            pass
        elif c == 0x10:
            p += 6
        elif c == 0x11:
            n = d[p:p + 4].decode("latin-1")
            if n not in names:
                names.append(n)
            p += 4
            ptr()
            p += 4
        elif c == 0x12:
            ptr()
            p += 5
        elif c in (0x13, 0x15):
            ptr()
            ptr()
            p += 4
        elif c == 0x14:
            ptr()
            p += 2 + struct.unpack(">H", d[p:p + 2])[0]
        elif c == 0x16:
            ptr()
            ptr()
        elif c == 0x17:
            mask = d[p + 18]
            p += 19
            for i in range(4):
                if mask & (1 << i):
                    ptr()
            p += 6
        elif c == 0x18:
            ptr()
        elif c == 0x19:
            p += 4
        elif c in (0x1C, 0x1D, 0x1E):
            p += 1
        elif c == 0x20:
            n = d[p]
            p += 1
            for _ in range(n):
                depth = d[p + 2]
                p += 3
                for _ in range(depth):
                    ptr()
                p += 14 + 64
        else:
            raise ValueError("record %02x at %d" % (c, p - 1))
    return names


def main():
    dest = sys.argv[1] if len(sys.argv) > 1 else os.path.join(OUT, "files")
    os.makedirs(dest, exist_ok=True)
    src = os.path.join(OUT, "intro.script")
    names = needed(open(src, "rb").read())
    shutil.copyfile(src, os.path.join(dest, "intro.script"))
    for n in names:
        for d in (DATA, DATA2):
            p = os.path.join(d, n)
            if os.path.exists(p):
                shutil.copyfile(p, os.path.join(dest, n))
                print("%-5s from %s (%d bytes)" % (n, os.path.basename(d), os.path.getsize(p)))
                break
        else:
            print("%-5s MISSING" % n)
    print("%d files + intro.script in %s" % (len(names), dest))


if __name__ == "__main__":
    main()
