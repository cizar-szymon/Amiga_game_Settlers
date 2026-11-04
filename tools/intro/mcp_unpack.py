"""Unpack the crunched 'mcp' executable by running its own decrunch stub in a 68000 emulator.

mcp is a crunched executable (Imploder-style backward LZ, the stub in hunks 0/2, packed data in
hunk 3). The stub decrunches into BSS hunk 4, then relocates the real program into BSS hunk 1
and "returns" into hunk 1. We emulate it with unicorn until the PC reaches hunk 1 and dump it.

Output: build/intro/raw/mcp_main.bin (the real program, relocated for base address BASE1)
        build/intro/asm/mcp_main.s   (disassembly at that base)
"""
import os
import unicorn as U
from unicorn import m68k_const as R
from introlib import load, outdir, be32
from disasm import hunks, dis

BASE = [0x20000, 0x30000, 0x40000, 0x50000, 0x60000]  # data address of each hunk
BASE1 = BASE[1]
EXEC = 0x8000
STACK = 0xF000


def main():
    buf = load("mcp", packed=False)
    sizes, hs = hunks(buf)
    mu = U.Uc(U.UC_ARCH_M68K, U.UC_MODE_BIG_ENDIAN)
    mu.ctl_set_cpu_model(R.UC_CPU_M68K_M68040)  # default model is ColdFire
    mu.mem_map(0, 0x100000)
    # exec: library vectors are plain rts, version 36 (so no CacheClearU), ThisTask -> zeros
    mu.mem_write(EXEC - 0x400, b"\x4e\x75" * 0x200)
    mu.mem_write(4, EXEC.to_bytes(4, "big"))
    mu.mem_write(EXEC + 0x14, (36).to_bytes(2, "big"))
    mu.mem_write(EXEC + 0x114, (0x9000).to_bytes(4, "big"))
    for i, (t, data, rel) in enumerate(hs):
        d = BASE[i]
        size = sizes[i] * 4
        mu.mem_write(d - 8, (size + 8).to_bytes(4, "big"))
        nxt = ((BASE[i + 1] - 4) >> 2) if i + 1 < len(hs) else 0
        mu.mem_write(d - 4, nxt.to_bytes(4, "big"))
        if data:
            mu.mem_write(d, data)
    for i, (t, data, rel) in enumerate(hs):
        for tgt, offs in rel:
            for o in offs:
                v = int.from_bytes(mu.mem_read(BASE[i] + o, 4), "big") + BASE[tgt]
                mu.mem_write(BASE[i] + o, v.to_bytes(4, "big"))
    mu.reg_write(R.UC_M68K_REG_A7, STACK)
    mu.mem_write(STACK, (0x7FFF0).to_bytes(4, "big"))  # fake return address
    last = []
    mu.hook_add(U.UC_HOOK_CODE, lambda uc, a, s, u: (last.append(a), last.__delitem__(0) if len(last) > 20 else None))
    try:
        mu.emu_start(BASE[0], BASE1, count=50_000_000)
    except U.UcError as e:
        print("error", e, " ".join("%x" % a for a in last))
        raise
    pc = mu.reg_read(R.UC_M68K_REG_PC)
    print("stopped at %x" % pc)
    prog = bytes(mu.mem_read(BASE1, sizes[1] * 4))
    with open(os.path.join(outdir("raw"), "mcp_main.bin"), "wb") as f:
        f.write(prog)
    with open(os.path.join(outdir("asm"), "mcp_main.s"), "w") as f:
        f.write("; mcp real program, base %x, size %d\n" % (BASE1, len(prog)))
        f.write(dis(prog, BASE1))
    print("mcp_main: %d bytes" % len(prog))


if __name__ == "__main__":
    main()
