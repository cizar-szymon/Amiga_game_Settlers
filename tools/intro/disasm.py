"""Disassemble the intro code (mcp hunk file and RLE-packed MASSIVE modules) with capstone.

usage: python disasm.py NAME [start_hex]     -> build/intro/asm/NAME.s
Modules (DISP, SD00, ...) are unpacked and disassembled as one flat block from offset 0
(header longword = size of the bra jump table area, then bra.w entries).
mcp is split into its hunks; each hunk is written to build/intro/asm/mcp.hunkN.s.
"""
import os
import sys
import capstone
from introlib import load, outdir, be32


def dis(code, base=0):
    md = capstone.Cs(capstone.CS_ARCH_M68K, capstone.CS_MODE_M68K_000 | capstone.CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    lines = []
    for ins in md.disasm(code, base):
        hx = code[ins.address - base: ins.address - base + ins.size].hex()
        txt = ""
        if ins.mnemonic in (".byte",) or ins.size >= 2:
            pass
        a = "".join(chr(c) if 32 <= c < 127 else "." for c in ins.bytes)
        lines.append("%06x  %-20s %-8s %s    ; %s" % (ins.address, hx, ins.mnemonic, ins.op_str, a))
    return "\n".join(lines) + "\n"


def hunks(buf):
    assert be32(buf, 0) == 0x3F3
    p = 4
    while be32(buf, p):  # resident library names
        p += 4 + 4 * be32(buf, p)
    p += 4
    n = be32(buf, p)
    first = be32(buf, p + 4)
    last = be32(buf, p + 8)
    p += 12
    sizes = [be32(buf, p + 4 * i) & 0x3FFFFFFF for i in range(last - first + 1)]
    p += 4 * len(sizes)
    out = []
    while p < len(buf):
        t = be32(buf, p) & 0x3FFFFFFF
        p += 4
        if t in (0x3E9, 0x3EA):  # CODE / DATA
            ln = be32(buf, p) * 4
            out.append(("CODE" if t == 0x3E9 else "DATA", buf[p + 4: p + 4 + ln], []))
            p += 4 + ln
        elif t == 0x3EB:  # BSS
            out.append(("BSS", b"", []))
            p += 4
        elif t == 0x3EC:  # RELOC32
            while True:
                cnt = be32(buf, p)
                p += 4
                if cnt == 0:
                    break
                tgt = be32(buf, p)
                offs = [be32(buf, p + 4 + 4 * i) for i in range(cnt)]
                out[-1][2].append((tgt, offs))
                p += 4 + 4 * cnt
        elif t == 0x3F2:  # END
            pass
        else:
            raise ValueError("hunk type %x at %x" % (t, p - 4))
    return sizes, out


def main():
    name = sys.argv[1]
    d = outdir("asm")
    if name == "mcp" or name in ("Loader", "Introecho", "Install"):
        buf = load(name, packed=False)
        sizes, hs = hunks(buf)
        for i, (t, data, rel) in enumerate(hs):
            fn = os.path.join(d, "%s.hunk%d.s" % (name, i))
            with open(fn, "w") as f:
                f.write("; hunk %d %s size %d\n" % (i, t, len(data)))
                for tgt, offs in rel:
                    f.write("; reloc -> hunk %d at %s\n" % (tgt, " ".join("%x" % o for o in offs)))
                if t == "CODE":
                    f.write(dis(data))
                elif t == "DATA":
                    f.write(data.hex() + "\n")
            print(fn, t, len(data))
    else:
        buf = load(name)
        start = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0
        fn = os.path.join(d, name + ".s")
        with open(fn, "w") as f:
            f.write(dis(buf[start:], start))
        print(fn)


if __name__ == "__main__":
    main()
