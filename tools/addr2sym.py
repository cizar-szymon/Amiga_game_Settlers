"""Maps run-time code addresses of the Amiga build to function names.

usage: python tools/addr2sym.py NM_OUTPUT MARKER_SYMBOL MARKER_RUNTIME ADDR...
NM_OUTPUT is `m68k-amigaos-nm -n -C build/amiga/<prog>` saved to a file; the
marker (e.g. hg_textmark, whose run-time address the heap guard logs) gives
the load offset of the code hunk.
"""
import sys


def main():
    nm_file, marker, marker_rt = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    syms = []
    marker_addr = None
    for line in open(nm_file, encoding="latin-1"):
        parts = line.rstrip("\n").split(" ", 2)
        if len(parts) < 3 or parts[1] not in "tTwW":
            continue
        addr = int(parts[0], 16)
        syms.append((addr, parts[2]))
        if parts[2] == marker or parts[2] == "_" + marker:
            marker_addr = addr
    if marker_addr is None:
        sys.exit("marker %s not found" % marker)
    syms.sort()
    delta = marker_rt - marker_addr
    for a in sys.argv[4:]:
        addr = int(a, 16) - delta
        best = None
        for s_addr, name in syms:
            if s_addr <= addr:
                best = (s_addr, name)
            else:
                break
        if best:
            print("%s -> %s+0x%x" % (a, best[1], addr - best[0]))
        else:
            print("%s -> ?" % a)


if __name__ == "__main__":
    main()
