"""Shared helpers for the Settlers (Amiga, Blue Byte 1993) intro tools.

All files on the intro disk (disk 1) are packed with a simple byte RLE:
    byte 0           escape byte E
    E n v            -> (n + 1) copies of byte v
    any other byte   -> copied as is
The unpacked size is not stored; the stream simply ends.
"""
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = os.path.join(ROOT, "data", "disk1")
DATA2 = os.path.join(ROOT, "data", "disk2")  # the intro continues on disk 2 (GA03, PIB5, KPIC...)
OUT = os.path.join(ROOT, "build", "intro")


def unrle(buf):
    esc = buf[0]
    out = bytearray()
    i = 1
    n = len(buf)
    while i < n:
        b = buf[i]
        if b == esc:
            if i + 2 >= n:
                break  # truncated escape at the very end
            cnt = buf[i + 1] + 1
            out += bytes([buf[i + 2]]) * cnt
            i += 3
        else:
            out.append(b)
            i += 1
    return bytes(out)


def load(name, packed=True):
    p = os.path.join(DATA, name)
    if not os.path.exists(p):
        p = os.path.join(DATA2, name)
    with open(p, "rb") as f:
        raw = f.read()
    return unrle(raw) if packed else raw


def outdir(*parts):
    p = os.path.join(OUT, *parts)
    os.makedirs(p, exist_ok=True)
    return p


def be16(b, o):
    return (b[o] << 8) | b[o + 1]


def be32(b, o):
    return (b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]


def s16(v):
    return v - 0x10000 if v & 0x8000 else v
