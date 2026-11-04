"""Print the jump table of a MASSIVE module: library offset (from module base) -> code offset."""
import sys
from introlib import load, be16, be32, s16

for name in sys.argv[1:]:
    b = load(name)
    hdr = be32(b, 0)
    print("== %s header %x" % (name, hdr))
    for o in range(4, hdr + 4, 4):
        if be16(b, o) == 0x6000:
            print("  -$%02x (file %02x) -> %04x" % (hdr - o, o, o + 2 + s16(be16(b, o + 2))))
