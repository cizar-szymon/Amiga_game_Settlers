"""Pack the recorded intro script (timeline.json + views.json from emu.py) into one binary file
for the C++ player: build/intro/intro.script (big endian). Prints sizes and the share of literal
bytes (pokes), so we can judge what the script contains.

Format (version 1):
  "STIS" u16 version u16 nblocks u32 nframes, then u32 size per block
  records: u8 code + fields; pointers are u16 block + s32 offset (may be slightly negative),
           block 0xFFFF = static data in a code module: name4 + u32 offset
  0x01 FRAME     u16 frame                 (all following records belong to this frame)
  0x02 IRQ                                  (following records run before the effect mixer)
  0x03 MAIN                                 (following records run after it)
  0x10 ALLOC     u16 block u32 size
  0x11 LOAD      name4 ptr u32 n            file (RLE unpacked, PICx raw) -> ptr
  0x12 FILL      ptr u32 n u8 value
  0x13 COPY      src ptr, dst ptr, u32 n
  0x14 POKE      ptr u16 n, n bytes
  0x15 UNPACK    src ptr, dst ptr, u32 n    ByteRun1
  0x16 DELTA     src ptr, dst ptr           GA03/GA04 frame differences
  0x17 BLIT      u16 con0 con1 size fwm lwm, s16 mod[4], u8 used-mask, ptr per used channel
                 (A B C D), u16 dat[3]
  0x18 SFXBANK   ptr
  0x19 SFX       u8 sample u8 volume u16 repeats
  0x1A SFXON  0x1B SFXOFF
  0x1C MUSIC     u8 op (1 play, 2 fade out, 3 stop)
  0x1D SCENE     u8 n
  0x1E SFXSTOP   u8 voice                   (volume 0, no repeats)
  0x20 VIEW      u8 nbands, per band: u16 first line, u8 depth, ptr per plane, u16 con1 ddfstrt
                 ddfstop, s16 mod1 mod2, u16 diwstrt diwstop, u16 colours[32]
                 (only when different from the previous frame)
"""
import json
import os
import struct
import sys
import zlib
from introlib import OUT
from player_ref import fix_pointers


def ptr(p):
    if p[0] == "mod":
        return struct.pack(">H4sI", 0xFFFF, p[1].encode("latin-1"), p[2])
    if p[0] == "abs":
        raise ValueError("unrelocated pointer %r" % (p,))
    return struct.pack(">Hi", p[0], p[1])


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "emu"
    d = os.path.join(OUT, src)
    tl = json.load(open(os.path.join(d, "timeline.json")))
    views = json.load(open(os.path.join(d, "views.json")))
    fix_pointers(tl)
    blocks = tl["blocks"]
    data_blocks = [b for b in blocks]
    out = bytearray(b"STIS" + struct.pack(">HHI", 1, len(data_blocks), len(views)))
    for a, size, mod in data_blocks:
        out += struct.pack(">I", size if not mod else 0)
    stats = {}
    poke_bytes = 0
    ops = tl["ops"]
    oi = 0
    last_view = None
    for f in range(len(views)):
        out += struct.pack(">BH", 0x01, f)
        phase = None
        while oi < len(ops) and ops[oi][0] <= f:
            o = ops[oi]
            oi += 1
            irq = o[0] < f or (o[0] != int(o[0]))
            if irq != phase:
                out.append(0x02 if irq else 0x03)
                phase = irq
            k = o[1]
            start = len(out)
            if k == "alloc":
                out += struct.pack(">BHI", 0x10, o[2], o[3])
            elif k == "load":
                out += struct.pack(">B4s", 0x11, o[2].encode()) + ptr(o[3]) + struct.pack(">I", o[4])
            elif k == "fill":
                out += bytes([0x12]) + ptr(o[2]) + struct.pack(">IB", o[3], o[4])
            elif k == "copy":
                out += bytes([0x13]) + ptr(o[2]) + ptr(o[3]) + struct.pack(">I", o[4])
            elif k == "poke":
                data = bytes.fromhex(o[3])
                for i in range(0, len(data), 0xFFFF):
                    part = data[i:i + 0xFFFF]
                    out += bytes([0x14]) + ptr([o[2][0], o[2][1] + i]) + struct.pack(">H", len(part)) + part
                poke_bytes += len(data)
            elif k == "unpack":
                out += bytes([0x15]) + ptr(o[2]) + ptr(o[3]) + struct.pack(">I", o[4])
            elif k == "delta":
                out += bytes([0x16]) + ptr(o[2]) + ptr(o[3])
            elif k == "blit":
                con0, con1, size, fwm, lwm, mods, ptrs, dat = o[2:10]
                mask = sum(1 << i for i in range(4) if ptrs[i] is not None)
                out += struct.pack(">BHHHHHhhhhB", 0x17, con0, con1, size, fwm, lwm, *mods, mask)
                for p in ptrs:
                    if p is not None:
                        out += ptr(p)
                out += struct.pack(">HHH", *dat)
            elif k == "sfxbank":
                out += bytes([0x18]) + ptr(o[2])
            elif k == "sfx":
                out += struct.pack(">BBBH", 0x19, o[2], o[3], o[4])
            elif k == "sfx_on":
                out.append(0x1A)
            elif k == "sfx_stop":
                out += struct.pack(">BB", 0x1E, o[2])
            elif k == "sfx_off":
                out.append(0x1B)
            elif k == "music":
                code = {"play": 1, "fade": 2, "stop": 3, "stop_now": 3}.get(o[2])
                if code:
                    out += struct.pack(">BB", 0x1C, code)
            elif k == "scene":
                out += struct.pack(">BB", 0x1D, o[2])
            stats[k] = stats.get(k, 0) + len(out) - start
        v = views[f]
        if v != last_view:
            start = len(out)
            out += struct.pack(">BB", 0x20, len(v))
            for oy, planes, depth, con1, ddfs, m1, m2, pal, dstrt, dstop, ddfe in v:
                out += struct.pack(">HB", oy, depth)
                for p in planes:
                    out += ptr(p)
                out += struct.pack(">HHHhhHH", con1, ddfs, ddfe, m1, m2, dstrt, dstop)
                out += struct.pack(">32H", *pal)
            stats["view"] = stats.get("view", 0) + len(out) - start
            last_view = v
    fn = os.path.join(OUT, "intro.script")
    open(fn, "wb").write(out)
    z = zlib.compress(bytes(out), 9)
    print("%s: %d bytes, zlib %d bytes, frames %d" % (fn, len(out), len(z), len(views)))
    for k, n in sorted(stats.items(), key=lambda x: -x[1]):
        print("  %-8s %8d bytes" % (k, n))
    print("  literal bytes in pokes: %d" % poke_bytes)


if __name__ == "__main__":
    main()
