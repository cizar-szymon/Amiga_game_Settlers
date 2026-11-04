"""Run the original intro code (SD00 + DISP, USER, EFEC, FUNC, FHRS) in a 68k emulator and record it.

This is the measuring instrument for the choreography, not part of the port:
  - real MASSIVE modules from the disks, unpacked, mcp base pointer patched in,
  - a fake mcp (memory, files, modules, interrupt servers) in Python,
  - fake SOUN (music) and INPT modules that only log calls,
  - custom chips: beam counter, blitter (area mode, numpy), copper list interpreter that renders
    the PAL display window (320x256 from DIW 0x81,0x2c) at every vertical blank,
  - interrupts injected between instruction slices: VERTB and COPER servers once per frame,
    AUD3 (SD00 sample mixer) once per frame while audio channel 3 runs.
The left mouse button is never pressed; the disk-2 marker FDD2 "exists" (no disk prompts).

Output (build/intro/emu/):
  frames/NNNNN.png   one picture per 50 Hz frame (PAL window 320x256)
  log.json           events with frame numbers: scene starts, module calls (blits, fades,
                     displays), sample triggers, music start/fade
  views.json         per frame: list of [first line, plane pointers, depth, BPLCON1, DDFSTRT,
                     BPL1MOD, BPL2MOD, 32 colours, DIWSTRT, DIWSTOP, DDFSTOP] - a new entry whenever the next line does not
                     simply continue the previous one (scroll split, palette change)
  sfx.raw            channel-3 mixer output, signed 8-bit, 160 bytes per frame (SD00 $558)

usage: python emu.py [--max-frames N] [--no-png] [--out DIR under build/intro, default emu]
"""
import bisect
import json
import os
import sys
import numpy as np
import unicorn as U
from unicorn import m68k_const as R
from introlib import load, outdir, be32, DATA, DATA2

RAM = 0x800000
EXEC = 0x2000            # ExecBase (not used by the modules; vectors are rts)
FLAG = 0x2F00            # byte tested by the trampolines (0 -> Z set = success)
TRAMP = 0x3000           # trampolines for fake library vectors
MCP = 0x8000             # fake mcp base
FAKEMOD = {"SOUN": 0xA000, "INPT": 0xB000}
STUB = 0xC000            # interrupt stub
MCPPTR = 0xCE00          # long holding the mcp base; modules do "movea.l MCPPTR,a6"
SENTINEL = 0xCF00        # return address for top-level calls -> stop
STACK = 0x1FF00
HEAP0 = 0x20000
MODBASE = 0x700000       # modules (code + variables) live here, data blocks below
LINES = 312
INSTR_PER_LINE = 300     # CPU speed model: ~94k instructions per frame

REAL_MODULES = ["FUNC", "FHRS", "DISP", "EFEC", "USER", "SD00"]
SCENES = {0xF70: 1, 0x1A28: 2, 0x5098: 3, 0x5C6E: 4, 0x60AA: 5, 0x35EE: 6, 0x6704: 7}


def rd16(mu, a):
    return int.from_bytes(mu.mem_read(a, 2), "big")


def rd16s(mu, a):
    v = rd16(mu, a)
    return v - 0x10000 if v & 0x8000 else v


def rd32(mu, a):
    return int.from_bytes(mu.mem_read(a, 4), "big")


def wr32(mu, a, v):
    mu.mem_write(a, (v & 0xFFFFFFFF).to_bytes(4, "big"))


def file_bytes(name):
    """Contents as mcp delivers them: RLE unpacked, except PICx (ByteRun1, unpacked by DISP)."""
    if name.startswith("PIC"):
        return load(name, packed=False)
    return load(name)


def file_exists(name):
    return os.path.exists(os.path.join(DATA, name)) or os.path.exists(os.path.join(DATA2, name))


class Emu:
    def __init__(self, max_frames=20000, png=True, out="emu"):
        self.mu = mu = U.Uc(U.UC_ARCH_M68K, U.UC_MODE_BIG_ENDIAN)
        mu.ctl_set_cpu_model(R.UC_CPU_M68K_M68040)    # the default model is ColdFire
        mu.mem_map(0, RAM)
        mu.mmio_map(0xDFF000, 0x1000, self.cust_read, None, self.cust_write, None)
        mu.mmio_map(0xBFD000, 0x2000, self.cia_read, None, self.cia_write, None)
        self.max_frames = max_frames
        self.png = png
        self.out = out
        self.framedir = outdir(out, "frames")
        self.heap = HEAP0
        self.custom = [0] * 0x100          # custom chip registers (words)
        self.dmacon = 0
        self.line = 0
        self.frame = 0
        self.in_irq = False
        self.servers = {}                 # intnum -> [(struct, code)]
        self.blocks = []                  # (addr, size, module name or "")
        self.block_starts = []             # sorted starts and the matching block ids
        self.block_ids = []
        self.timeline = []
        self.modheap = MODBASE
        self.unknown_writes = {}
        self.reads = []
        self.run_pending = None
        self.stubs = {}
        self.modules = {}
        self.modinfo = {}
        self.events = []
        self.sfx = bytearray()
        self.views = []
        self.aud3 = 0
        self.done = False
        self.sp = STACK
        self.res = {}                     # mcp named resources: name -> [addr, size, refs]
        # exec, mcp and the fake modules: each 4-byte vector is "jmp TRAMP.w"; the trampoline is
        # "tst.b FLAG.w; rts" and a hook on it does the work in Python and sets FLAG for the
        # Z flag the callers test (writing SR from a hook is not reliable in unicorn).
        mu.mem_write(EXEC - 0x400, bytes([0x4E, 0x75]) * 0x200)
        wr32(mu, 4, EXEC)
        wr32(mu, MCPPTR, MCP)
        wr32(mu, MCP + 0, 0x7000)        # memory pool handle (opaque)
        wr32(mu, MCP + 0x8C, 1)          # "disk 2 available"
        self.tramp = {}
        t = TRAMP
        for area, base in [("mcp", MCP)] + list(FAKEMOD.items()):
            for off in range(4, 0x400, 4):
                mu.mem_write(base - off, bytes([0x4E, 0xF8]) + t.to_bytes(2, "big"))
                mu.mem_write(t, bytes([0x4A, 0x38]) + FLAG.to_bytes(2, "big") + bytes([0x4E, 0x75]))
                self.tramp[t] = (area, off)
                t += 8
        assert t < 0x7C00
        mu.hook_add(U.UC_HOOK_CODE, self.hook_tramp, begin=TRAMP, end=t)
        mu.mem_write(SENTINEL, bytes([0x4E, 0x75]))
        mu.hook_add(U.UC_HOOK_CODE, self.hook_exec, begin=EXEC - 0x400, end=EXEC - 1)
        mu.hook_add(U.UC_HOOK_CODE, self.hook_sentinel, begin=SENTINEL, end=SENTINEL)
        mu.reg_write(R.UC_M68K_REG_SR, 0x2000)          # supervisor, interrupts enabled
        for n in REAL_MODULES:
            self.load_module(n)
        self.add_trace_hooks()
        mu.hook_add(U.UC_HOOK_MEM_WRITE, self.hook_write, begin=HEAP0, end=MODBASE - 1)
        mu.hook_add(U.UC_HOOK_MEM_READ, self.hook_read, begin=HEAP0, end=MODBASE - 1)
        # writers handled as semantic ops (or audio buffers): not captured as copies/pokes
        self.skip_pc = []
        for mod, a, b in [("DISP", 0x111C, 0x1190), ("SD00", 0x6076, 0x60AA),
                          ("SD00", 0x62C8, 0x62FA), ("SD00", 0x3A6, 0x3C4),
                          ("SD00", 0x50A, 0x602)]:
            base = self.modinfo[mod][0]
            self.skip_pc.append((base + a, base + b))

    # ---------------------------------------------------------------- memory, modules
    def alloc(self, size, module=None):
        """Bump allocator. Data blocks are recorded for the timeline (block id = index)."""
        if module:
            a = (self.modheap + 15) & ~15
            self.modheap = a + size + 16
            assert self.modheap < RAM
        else:
            a = (self.heap + 15) & ~15
            self.heap = a + size + 16
            assert self.heap < MODBASE, "out of memory"
        self.mu.mem_write(a, bytes(size))
        self.blocks.append((a, size, module or ""))
        i = bisect.bisect_right(self.block_starts, a)
        self.block_starts.insert(i, a)
        self.block_ids.insert(i, len(self.blocks) - 1)
        if not module:
            self.op("alloc", len(self.blocks) - 1, size)
        return a

    def reloc(self, addr):
        """Address -> [block, offset] (block = index in self.blocks), or ["abs", addr]."""
        j = bisect.bisect_right(self.block_starts, addr) - 1
        if j >= 0:
            i = self.block_ids[j]
            a, size, mod = self.blocks[i]
            if addr < a + size + 16:
                return [i, addr - a] if not mod else ["mod", mod, addr - a]
        if j + 1 < len(self.block_starts) and self.block_starts[j + 1] - addr <= 64:
            i = self.block_ids[j + 1]                 # just before a block (blits starting one
            a, size, mod = self.blocks[i]              # word left of a buffer): negative offset
            if not mod:
                return [i, addr - a]
        return ["abs", addr]

    def now(self):
        """Op time: frame number; ops from interrupt handlers (run at the start of the frame,
        before the effect mixer) get frame - 0.5."""
        return self.frame - 0.5 if self.in_irq else self.frame

    def op(self, name, *args):
        self.flush_run()
        self.timeline.append([self.now(), name] + list(args))

    def flush_run(self):
        r = self.run_pending
        if r:
            self.run_pending = None
            if r[0] == "copy":
                self.timeline.append([r[4], "copy", self.reloc(r[1]), self.reloc(r[2]), r[3]])
            else:
                self.timeline.append([r[4], "poke", self.reloc(r[1]), bytes(r[2]).hex()])

    def load_module(self, name):
        b = bytearray(load(name))
        hdr = be32(b, 0)
        for i in range(0, len(b) - 5, 2):
            if b[i:i + 6] == bytes([0x2C, 0x79, 0, 0, 0, 0]):
                b[i + 2:i + 6] = MCPPTR.to_bytes(4, "big")
        addr = self.alloc(len(b) + 0x1000, module=name)
        self.mu.mem_write(addr, bytes(b))
        base = addr + hdr
        self.modules[name] = base
        self.modinfo[name] = (addr, len(b))
        self.call(base - 0x10, a6=base)       # module init
        return base

    def call(self, pc, **regs):
        """Run a subroutine from the top level until it returns."""
        mu = self.mu
        sp = self.sp - 4
        wr32(mu, sp, SENTINEL)
        mu.reg_write(R.UC_M68K_REG_A7, sp)
        for k, v in regs.items():
            mu.reg_write(getattr(R, "UC_M68K_REG_" + k.upper()), v)
        mu.reg_write(R.UC_M68K_REG_PC, pc)
        self.done = False
        self.run()

    # ---------------------------------------------------------------- main loop
    def run(self):
        mu = self.mu
        while not self.done:
            pc = mu.reg_read(R.UC_M68K_REG_PC)
            try:
                mu.emu_start(pc, 0xFFFFFFFF, count=INSTR_PER_LINE)
            except U.UcError as e:
                raise RuntimeError("CPU error %s at %x (frame %d)" % (e, mu.reg_read(R.UC_M68K_REG_PC),
                                                                      self.frame))
            if self.done:
                break
            self.line += 1
            if self.line >= LINES:
                self.line = 0
                self.vblank()

    def vblank(self):
        self.render()
        self.frame += 1
        if self.frame >= self.max_frames:
            raise SystemExit("max frames")
        if self.frame % 500 == 0:
            print("frame", self.frame, file=sys.stderr)
        aud = 10 in self.servers and (self.dmacon & 0x208) == 0x208
        # Paula plays the buffer set in AUD3LC; take it before the mixer refills the other one
        self.sfx += bytes(self.mu.mem_read(self.aud3, 160)) if aud and self.aud3 else bytes(160)
        if self.in_irq:
            return
        codes = []
        for intnum in (5, 4):                    # VERTB, COPER
            codes += [(c, s) for s, c in self.servers.get(intnum, [])]
        if aud:
            codes += [(c, s) for s, c in self.servers[10]]
        if codes:
            self.inject(codes)

    def inject(self, codes):
        mu = self.mu
        code = bytearray([0x40, 0xE7])                                  # move.w sr,-(a7)
        for c, s in codes:
            code += bytes([0x48, 0xE7, 0xFF, 0xFE])                      # movem.l d0-d7/a0-a6,-(a7)
            data = rd32(self.mu, s + 0xC)                               # server data (+$c)
            code += bytes([0x22, 0x7C]) + data.to_bytes(4, "big")        # movea.l #data,a1
            code += bytes([0x4E, 0xB9]) + c.to_bytes(4, "big")           # jsr c
            code += bytes([0x4C, 0xDF, 0x7F, 0xFF])                      # movem.l (a7)+,d0-d7/a0-a6
        code += bytes([0x33, 0xFC, 0x00, 0x01, 0x00, 0xDF, 0xF1, 0xFE])  # move.w #1,$dff1fe (end)
        code += bytes([0x46, 0xDF])                                      # move.w (a7)+,sr
        code += bytes([0x4E, 0x75])                                      # rts
        # one stub per handler list, written once: unicorn keeps translated code cached and does
        # not notice host-side rewrites of code it has already run
        key = bytes(code)
        if key not in self.stubs:
            addr = STUB + sum(len(k) + 2 for k in self.stubs)
            addr = (addr + 3) & ~3
            assert addr + len(code) < MCPPTR
            mu.mem_write(addr, key)
            self.stubs[key] = addr
        stub = self.stubs[key]
        # The flags must be saved by the CPU itself (move.w sr in the stub): unicorn's lazy
        # condition codes are not reliable through reg_read(SR) between slices.
        pc = mu.reg_read(R.UC_M68K_REG_PC)
        sp = mu.reg_read(R.UC_M68K_REG_A7) - 4
        wr32(mu, sp, pc)
        mu.reg_write(R.UC_M68K_REG_A7, sp)
        mu.reg_write(R.UC_M68K_REG_PC, stub)
        self.in_irq = True

    # ---------------------------------------------------------------- hooks
    def ev(self, kind, **kw):
        kw["f"] = self.frame
        kw["k"] = kind
        self.events.append(kw)

    def ret(self, d0=None, z=True, **regs):
        mu = self.mu
        if d0 is not None:
            mu.reg_write(R.UC_M68K_REG_D0, d0 & 0xFFFFFFFF)
        for k, v in regs.items():
            mu.reg_write(getattr(R, "UC_M68K_REG_" + k.upper()), v & 0xFFFFFFFF)
        mu.mem_write(FLAG, bytes([0 if z else 1]))

    @staticmethod
    def name4(v):
        return v.to_bytes(4, "big").decode("latin-1")

    def hook_sentinel(self, mu, addr, size, ud):
        self.sp = mu.reg_read(R.UC_M68K_REG_A7) + 4
        self.done = True
        mu.emu_stop()

    def hook_exec(self, mu, addr, size, ud):
        raise RuntimeError("exec call -$%x" % (EXEC - addr))

    def hook_tramp(self, mu, addr, size, ud):
        if addr not in self.tramp:
            return                               # the rts of a trampoline
        area, off = self.tramp[addr]
        if area == "mcp":
            self.hook_mcp(mu, off)
        else:
            self.hook_fake(mu, area, off)

    def hook_fake(self, mu, name, off):
        d0 = mu.reg_read(R.UC_M68K_REG_D0)
        if name == "SOUN":
            what = {0x24: "enable", 0x34: "play", 0x3C: "fade", 0x38: "stop_now",
                    0x20: "stop"}.get(off, "?")
            self.ev("music", op=what, off=off, d0=d0)
            self.op("music", what, d0)
        else:
            self.ev("input", off=off)
        self.ret(0)

    def hook_mcp(self, mu, off):
        g = lambda r: mu.reg_read(getattr(R, "UC_M68K_REG_" + r))
        d0, d1, a0 = g("D0"), g("D1"), g("A0")
        if off == 0x18:                       # open module d0 -> d0 base
            n = self.name4(d0)
            base = self.modules.get(n) or FAKEMOD.get(n)
            if base is None:
                raise RuntimeError("module " + n)
            self.ret(base)
        elif off == 0x1C:                     # close module
            self.ret(0)
        elif off == 0x28:                     # alloc d0 bytes (d1 type, a0 pool) -> d0
            self.ret(self.alloc(d0 or 4))
        elif off == 0x30:                     # free
            self.ret(0)
        elif off == 0x34:                     # fill a0, d0 bytes with d1
            mu.mem_write(a0, bytes([d1 & 0xFF]) * d0)
            self.op("fill", self.reloc(a0), d0, d1 & 0xFF)
            self.ret(0)
        elif off == 0x38:                     # copy d0 bytes a0 -> a1
            mu.mem_write(g("A1"), bytes(mu.mem_read(a0, d0)))
            self.op("copy", self.reloc(a0), self.reloc(g("A1")), d0)
            self.ret(0)
        elif off == 0x40:                     # alloc (variant) d0 bytes -> d0
            self.ret(self.alloc(d0 or 4))
        elif off == 0x64:                     # load file d0 into a0 (d1 bytes), register by name
            n = self.name4(d0)
            b = file_bytes(n)[:d1]
            mu.mem_write(a0, b)
            self.res[n] = [a0, len(b), 1]
            self.ev("load", name=n, addr=a0, size=len(b))
            self.op("load", n, self.reloc(a0), len(b))
            self.ret(0)
        elif off == 0x48:                     # is resource d0 loaded? (Z = yes)
            n = self.name4(d0)
            if n in self.res:
                self.res[n][2] += 1
            self.ret(None, z=n in self.res)
        elif off == 0x44:                     # resource d0 -> d0 address (Z = found)
            n = self.name4(d0)
            self.ret(self.res[n][0] if n in self.res else 0, z=n in self.res)
        elif off in (0x50, 0x4C):             # release / unload resource d0
            n = self.name4(d0)
            if n in self.res:
                self.res[n][2] -= 1
                if self.res[n][2] <= 0:
                    del self.res[n]
            self.ret(0)
        elif off == 0x88:                     # find node with key d0 in list a0 -> a1 (Z = found)
            p = a0
            while p and rd32(mu, p + 4) != d0:
                p = rd32(mu, p)
            self.ret(None, z=p != 0, a1=p)
        elif off == 0x80:                     # new node (d0 bytes, key d1) in front of list a1
            node = self.alloc(max(d0, 0x16))
            wr32(mu, node, g("A1"))
            wr32(mu, node + 4, d1)
            self.ret(None, a1=node, a2=node)
        elif off == 0x84:                     # remove node with key d0 from list a1 -> a1
            head = g("A1")
            prev, p = 0, head
            while p and rd32(mu, p + 4) != d0:
                prev, p = p, rd32(mu, p)
            if p:
                if prev:
                    wr32(mu, prev, rd32(mu, p))
                else:
                    head = rd32(mu, p)
            self.ret(None, a1=head)
        elif off == 0x60:                     # file size d0 -> d1
            n = self.name4(d0)
            if not file_exists(n):
                self.ev("missing", name=n)
                self.ret(None, z=False)
            else:
                self.ret(None, d1=len(file_bytes(n)))
        elif off == 0x54:                     # load file d0 -> a0, d1 bytes
            n = self.name4(d0)
            b = file_bytes(n)[:d1]
            mu.mem_write(a0, b)
            self.ev("load", name=n, addr=a0, size=len(b))
            self.op("load", n, self.reloc(a0), len(b))
            self.ret(0)
        elif off == 0xC8:                     # file exists
            self.ret(0, z=file_exists(self.name4(d0)))
        elif off == 0x98:                     # add interrupt server a0 (code at +8), d0 intnum
            c = rd32(mu, a0 + 8)
            self.servers.setdefault(d0, []).append((a0, c))
            self.ev("irq_add", n=d0, code=c)
            self.ret(0)
        elif off == 0x9C:                     # remove interrupt server
            self.servers[d0] = [x for x in self.servers.get(d0, []) if x[0] != a0]
            self.ev("irq_rem", n=d0)
            self.ret(0)
        else:
            raise RuntimeError("mcp -$%x d0=%x d1=%x a0=%x a1=%x caller=%x frame %d" % (
                off, d0, d1, a0, g("A1"), rd32(mu, mu.reg_read(R.UC_M68K_REG_A7)), self.frame))

    def add_trace_hooks(self):
        """Log calls of interest (entry addresses inside the real modules)."""
        sd = self.modinfo["SD00"][0]
        self.sd00 = sd
        self.trace = {}
        for off, nm in [(0x41E, "sfx"), (0x602, "sfx_off"), (0x628, "sfx_on"), (0x402, "sfx_stop"),
                        (0x6076, "delta3"), (0x62C8, "delta4")]:
            self.trace[sd + off] = nm
        for routine, n in SCENES.items():
            self.trace[sd + routine] = "scene%d" % n
        self.trace[sd + 0xADE] = "music_fadeout"
        for off, nm in {0x54: "blit", 0x50: "blitpk", 0x4C: "unpack", 0x18: "disp_open",
                        0x1C: "disp_close", 0x20: "disp_show", 0x28: "disp_hide",
                        0x64: "maskcut"}.items():
            self.trace[self.jt_target("DISP", off)] = "DISP_" + nm
        for off in (0x24, 0x28, 0x2C, 0x30):
            self.trace[self.jt_target("EFEC", off)] = "EFEC_%x" % off
        for off in (0x2C, 0x4C, 0x50, 0x30):
            self.trace[self.jt_target("USER", off)] = "USER_%x" % off
        for a in self.trace:
            self.mu.hook_add(U.UC_HOOK_CODE, self.hook_trace, begin=a, end=a)

    def jt_target(self, mod, off):
        addr = self.modules[mod] - off
        w = rd16(self.mu, addr + 2)
        return addr + 2 + (w - 0x10000 if w & 0x8000 else w)

    def hook_trace(self, mu, addr, size, ud):
        nm = self.trace.get(addr)
        if nm is None:
            return
        g = lambda r: mu.reg_read(getattr(R, "UC_M68K_REG_" + r))
        regs = {r.lower(): g(r) for r in ("D0", "D1", "D2", "D3", "D4", "A0", "A1")}
        regs["ret"] = rd32(mu, g("A7")) - self.sd00
        if nm.startswith("DISP_") and regs["a0"]:
            regs["disp"] = self.disp_info(regs["a0"])
        self.ev(nm, **regs)
        # timeline (what a player needs besides the blits)
        if nm == "DISP_unpack":                  # ByteRun1 a0 -> a1, d0 bytes out
            self.op("unpack", self.reloc(regs["a0"]), self.reloc(regs["a1"]), regs["d0"])
        elif nm in ("delta3", "delta4"):         # frame-difference list a0 -> US display bitmap
            us = rd32(mu, self.modules["SD00"] + (0x28 if nm == "delta3" else 0x2C))
            disp = us + rd16(mu, us)
            base = rd32(mu, rd32(mu, disp + 0x1A))
            self.op("delta", self.reloc(regs["a0"]), self.reloc(base))
        elif nm == "sfx":
            self.op("sfxbank", self.reloc(rd32(mu, self.modules["SD00"] + 0x4C)))
            self.op("sfx", regs["d0"] & 0xFFFF, regs["d1"] & 0xFFFF, regs["d2"] & 0xFFFF)
        elif nm in ("sfx_on", "sfx_off"):
            self.op(nm)
        elif nm == "sfx_stop":                    # SD00 $402: voice d0 volume 0, no repeats
            self.op("sfx_stop", regs["d0"] & 0xFFFF)
        elif nm.startswith("scene"):
            self.op("scene", int(nm[5:]))


    def hook_read(self, mu, access, addr, size, value, ud):
        """Remember recent CPU reads from data blocks (to recognise copies)."""
        v = int.from_bytes(mu.mem_read(addr, size), "big")
        rq = self.reads
        rq.append((addr, size, v))
        if len(rq) > 32:
            del rq[0]

    def hook_write(self, mu, access, addr, size, value, ud):
        """Capture CPU writes into data blocks as copy runs (value read just before from a data
        block) or poke runs (literal bytes); count them by writer for cpu_writes.txt."""
        pc = mu.reg_read(R.UC_M68K_REG_PC)
        b = self.reloc(addr)[0]
        key = (self.where(pc), b)
        self.unknown_writes[key] = self.unknown_writes.get(key, 0) + 1
        for a0, a1 in self.skip_pc:
            if a0 <= pc < a1:
                return
        value &= (1 << (8 * size)) - 1
        src = None
        for i, (ra, rs, rv) in enumerate(self.reads):
            if rs == size and rv == value:
                src = ra
                del self.reads[:i + 1]
                break
        r = self.run_pending
        if src is not None:
            if r and r[0] == "copy" and r[1] + r[3] == src and r[2] + r[3] == addr:
                r[3] += size
                return
            self.flush_run()
            self.run_pending = ["copy", src, addr, size, self.now()]
        else:
            data = value.to_bytes(size, "big")
            if r and r[0] == "poke" and r[1] + len(r[2]) == addr:
                r[2] += data
                return
            self.flush_run()
            self.run_pending = ["poke", addr, bytearray(data), None, self.now()]

    def where(self, pc):
        for n, (a, ln) in self.modinfo.items():
            if a <= pc < a + ln:
                return "%s+%x" % (n, pc - a)
        return "%x" % pc

    def disp_info(self, s):
        """Display struct: bitmap base (first row pointer), bytes/row, height, depth."""
        mu = self.mu
        rows = rd32(mu, s + 0x1A)
        base = rd32(mu, rows) if 0 < rows < RAM else 0
        return [s, base, rd16(mu, s + 4), rd16(mu, s + 6), rd16(mu, s + 0x10)]

    # ---------------------------------------------------------------- custom chips
    def cust_read(self, mu, off, size, ud):
        vpos = self.line
        if off == 0x004:
            v = (vpos >> 8) & 1
            return ((v << 16) | ((vpos & 0xFF) << 8) | 0x40) if size == 4 else v
        if off == 0x005:
            return (vpos >> 8) & 1
        if off == 0x006:
            return ((vpos & 0xFF) << 8 | 0x40) if size == 2 else (vpos & 0xFF)
        if off == 0x002:                         # DMACONR, blitter never busy
            return self.dmacon & 0x3FFF if size == 2 else (self.dmacon >> 8) & 0x3F
        if off == 0x003:
            return self.dmacon & 0xFF
        return 0

    def cust_write(self, mu, off, size, value, ud):
        if size == 4:
            self.cust_write(mu, off, 2, (value >> 16) & 0xFFFF, ud)
            self.cust_write(mu, off + 2, 2, value & 0xFFFF, ud)
            return
        if size == 1:
            old = self.custom[off >> 1]
            value &= 0xFF
            value = (old & 0xFF00) | value if off & 1 else (value << 8) | (old & 0xFF)
            off &= ~1
        if off == 0x1FE:
            self.in_irq = False
            return
        if off == 0x096:
            self.dmacon = (self.dmacon | (value & 0x7FFF)) if value & 0x8000 else (self.dmacon & ~value)
            return
        self.custom[off >> 1] = value & 0xFFFF
        if off == 0x058:
            self.blit(mu.reg_read(R.UC_M68K_REG_PC))
        elif off == 0x0D2:
            self.aud3 = ((self.custom[0xD0 >> 1] << 16) | self.custom[0xD2 >> 1]) & 0xFFFFFE

    def cia_read(self, mu, off, size, ud):
        return 0xFF                              # $bfe001 bit 6 high: left button not pressed

    def cia_write(self, mu, off, size, value, ud):
        pass

    def reg(self, off):
        return self.custom[off >> 1]

    def ptr(self, off):
        return ((self.reg(off) << 16) | self.reg(off + 2)) & 0xFFFFFE

    def blit(self, pc=0):
        """Blitter area mode (A, B, C -> D), shifts, first/last word masks, ascending/descending."""
        mu = self.mu
        con0, con1 = self.reg(0x40), self.reg(0x42)
        if con1 & 1:
            self.ev("blit_line_unsupported")
            return
        size = self.reg(0x58)
        h = (size >> 6) or 1024
        w = (size & 0x3F) or 64
        desc = bool(con1 & 2)
        ash, bsh = con0 >> 12, con1 >> 12
        mint = con0 & 0xFF
        use = [(con0 >> (11 - i)) & 1 for i in range(4)]   # A B C D
        mods = []
        for o in (0x64, 0x62, 0x60, 0x66):
            m = self.reg(o) & 0xFFFE
            mods.append(m - 0x10000 if m & 0x8000 else m)
        ptrs = [self.ptr(0x50), self.ptr(0x4C), self.ptr(0x48), self.ptr(0x54)]
        dat = [self.reg(0x74), self.reg(0x72), self.reg(0x70)]
        fwm, lwm = self.reg(0x44), self.reg(0x46)
        n = w * h
        sgn = -1 if desc else 1

        def addrs(ch):
            col = np.arange(w, dtype=np.int64) * 2 * sgn
            row = np.arange(h, dtype=np.int64) * (2 * w + mods[ch]) * sgn
            return (ptrs[ch] + row[:, None] + col[None, :]).ravel()

        def fetch(ch):
            a = addrs(ch)
            lo, hi = int(a.min()), int(a.max()) + 2
            buf = np.frombuffer(bytes(mu.mem_read(lo, hi - lo)), dtype=np.uint8)
            i = a - lo
            return (buf[i].astype(np.uint32) << 8) | buf[i + 1]

        A = fetch(0) if use[0] else np.full(n, dat[0], np.uint32)
        B = fetch(1) if use[1] else np.full(n, dat[1], np.uint32)
        C = fetch(2) if use[2] else np.full(n, dat[2], np.uint32)
        col = np.tile(np.arange(w), h)
        A = A.copy()
        A[col == 0] &= fwm
        A[col == w - 1] &= lwm

        def shift(x, s):
            if not s:
                return x
            prev = np.concatenate(([0], x[:-1])).astype(np.uint32)
            if desc:
                return ((x << s) | (prev >> (16 - s))) & 0xFFFF
            return (((prev << 16) | x) >> s) & 0xFFFF

        As, Bs = shift(A, ash), shift(B, bsh)
        nA, nB, nC = ~As & 0xFFFF, ~Bs & 0xFFFF, ~C & 0xFFFF
        terms = [nA & nB & nC, nA & nB & C, nA & Bs & nC, nA & Bs & C,
                 As & nB & nC, As & nB & C, As & Bs & nC, As & Bs & C]
        D = np.zeros(n, np.uint32)
        for i in range(8):
            if mint & (1 << i):
                D |= terms[i]
        if use[3]:
            a = addrs(3).reshape(h, w)
            out = np.empty((h, w, 2), np.uint8)
            Dm = D.reshape(h, w)
            out[:, :, 0] = (Dm >> 8) & 0xFF
            out[:, :, 1] = Dm & 0xFF
            for r in range(h):
                if desc:
                    mu.mem_write(int(a[r, -1]), out[r, ::-1].tobytes())
                else:
                    mu.mem_write(int(a[r, 0]), out[r].tobytes())
        self.op("blit", con0, con1, size, fwm, lwm, mods,
                [self.reloc(ptrs[i]) if use[i] else None for i in range(4)], dat, pc)
        for ch, o in enumerate((0x50, 0x4C, 0x48, 0x54)):     # pointers continue
            if use[ch]:
                p = ptrs[ch] + sgn * h * (2 * w + mods[ch])
                self.custom[o >> 1] = (p >> 16) & 0xFFFF
                self.custom[(o + 2) >> 1] = p & 0xFFFF

    # ---------------------------------------------------------------- copper + display
    def render(self):
        """Interpret the copper list (COP1LC) line by line; save the PAL window as PNG."""
        from PIL import Image
        mu = self.mu
        regs = list(self.custom)
        cop = ((regs[0x80 >> 1] << 16) | regs[0x82 >> 1]) & 0xFFFFFE
        ins = []
        p = cop
        while cop and len(ins) < 4000:
            w1, w2 = rd16(mu, p), rd16(mu, p + 2)
            p += 4
            ins.append((w1, w2))
            if w1 == 0xFFFF and w2 == 0xFFFE:
                break
        rgb = np.zeros((256, 320, 3), np.uint8)
        view = []
        expect, vcur = -1, None
        ip = 0
        planes = [((regs[(0xE0 + 4 * k) >> 1] << 16) | regs[(0xE2 + 4 * k) >> 1]) & 0xFFFFFE
                  for k in range(6)]
        xs = np.arange(320)
        for y in range(LINES):
            while ip < len(ins):
                w1, w2 = ins[ip]
                if w1 & 1 == 0:                              # MOVE
                    rr = w1 & 0x1FE
                    regs[rr >> 1] = w2
                    if 0xE0 <= rr < 0xF8:
                        k = (rr - 0xE0) >> 2
                        planes[k] = ((regs[(0xE0 + 4 * k) >> 1] << 16) |
                                     regs[(0xE2 + 4 * k) >> 1]) & 0xFFFFFE
                    ip += 1
                elif w2 & 1 == 0:                            # WAIT
                    if w1 == 0xFFFF:
                        ip = len(ins)
                        break
                    vp, hp = w1 >> 8, w1 & 0xFE
                    yy = y - 1 if hp >= 0xD0 else y
                    if yy >= 0 and (yy & 0xFF) >= vp:
                        ip += 1
                    else:
                        break
                else:                                        # SKIP: not used
                    ip += 1
            oy = y - 0x2C
            if not 0 <= oy < 256:
                continue
            con0 = regs[0x100 >> 1]
            depth = (con0 >> 12) & 7
            dstrt, dstop = regs[0x8E >> 1], regs[0x90 >> 1]
            vs = dstrt >> 8
            ve = (dstop >> 8) | (0 if dstop & 0x8000 else 0x100)
            cols = np.array([[((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17]
                             for v in regs[0x180 >> 1:(0x180 >> 1) + 32]], np.uint8)
            cols = np.concatenate([cols, cols // 2])
            if not (depth and vs <= y < ve and (self.dmacon & 0x300) == 0x300):
                rgb[oy] = cols[0]
                continue
            ddfs, ddfe = regs[0x92 >> 1] & 0xFC, regs[0x94 >> 1] & 0xFC
            nwords = (ddfe - ddfs) // 8 + 1
            con1 = regs[0x102 >> 1]
            m1, m2 = regs[0x108 >> 1], regs[0x10A >> 1]
            m1 = m1 - 0x10000 if m1 & 0x8000 else m1
            m2 = m2 - 0x10000 if m2 & 0x8000 else m2
            pal32 = list(regs[0x180 >> 1:(0x180 >> 1) + 32])
            cur = [depth, con1, ddfs, m1, m2, pal32, dstrt, dstop, ddfe]
            if planes[0] != expect or cur != vcur:     # new band: jump in memory or new settings
                view.append([oy, [self.reloc(planes[k]) for k in range(depth)]] + cur)
            vcur = cur
            pix = np.zeros(nwords * 16 + 16, np.uint8)
            for k in range(depth):
                raw = np.frombuffer(bytes(mu.mem_read(planes[k], nwords * 2)), np.uint8)
                bits = np.unpackbits(raw)
                delay = (con1 >> (4 if k & 1 else 0)) & 0xF
                pix[delay:delay + bits.size] |= (bits << k).astype(np.uint8)
            for k in range(depth):
                planes[k] += nwords * 2 + (m2 if k & 1 else m1)
            expect = planes[0]
            if depth < 6 or con0 & 0x0C00:      # EHB only with 6 planes, no HAM/dual playfield
                pix &= 31
            x0 = ddfs * 2 + 17 - 0x81
            hs = (dstrt & 0xFF) - 0x81
            he = (dstop & 0xFF) + 0x100 - 0x81
            i = xs - x0
            ok = (i >= 0) & (i < pix.size) & (xs >= hs) & (xs < he)
            idx = np.where(ok, pix[np.clip(i, 0, pix.size - 1)], 0)
            rgb[oy] = cols[idx]
        self.views.append(view)
        if self.png:
            Image.fromarray(rgb).save(os.path.join(self.framedir, "%05d.png" % self.frame),
                                      compress_level=1)
        for r in range(0x100):                   # registers keep what the copper wrote
            if r not in (0x80 >> 1, 0x82 >> 1):
                self.custom[r] = regs[r]

    # ---------------------------------------------------------------- run the intro
    def run_intro(self):
        sd = self.modules["SD00"]
        self.ev("start")
        self.call(sd, a6=sd)
        self.ev("end")


def main():
    mf = int(sys.argv[sys.argv.index("--max-frames") + 1]) if "--max-frames" in sys.argv else 20000
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "emu"
    e = Emu(mf, "--no-png" not in sys.argv, out)
    try:
        e.run_intro()
    except SystemExit as x:
        print(x, file=sys.stderr)
    finally:
        od = outdir(e.out)
        with open(os.path.join(od, "log.json"), "w") as f:
            json.dump(e.events, f)
        with open(os.path.join(od, "views.json"), "w") as f:
            json.dump(e.views, f)
        e.flush_run()
        with open(os.path.join(od, "timeline.json"), "w") as f:
            json.dump({"blocks": [[a, sz, m] for a, sz, m in e.blocks], "ops": e.timeline,
                       "frames": e.frame}, f)
        with open(os.path.join(od, "cpu_writes.txt"), "w") as f:
            for (w, b), n in sorted(e.unknown_writes.items(), key=lambda x: -x[1]):
                blk = e.blocks[b] if isinstance(b, int) else None
                f.write("%-14s block %-5s %8d writes  %s\n" % (w, b, n, blk))
        with open(os.path.join(od, "sfx.raw"), "wb") as f:
            f.write(bytes(e.sfx))
        print("frames", e.frame, "events", len(e.events))


if __name__ == "__main__":
    main()
