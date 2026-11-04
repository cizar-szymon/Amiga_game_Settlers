"""Decode the pictures and animation frames of the Settlers Amiga intro to PNG.

Decoder library; run `python gfx_ga.py` for the export (build/intro/png/...).

Formats (see docs/INTRO.md):
  PICx   NOT RLE-packed on disk; ByteRun1 (PackBits) of an interleaved bitmap, palette in USx
  PIBx   RLE-packed (introlib.unrle); raw interleaved bitmap 320x256x6 (EHB), palette in SD00
  GAxx   RLE-packed archive: long total size, long offsets[]; entries are raw interleaved
         images (no header), dimensions are hard-coded in SD00 (see FRAMES below)
  KPIC/KMSK  RLE-packed raw interleaved image + 1-plane mask
"""
import os
import struct
from PIL import Image
from introlib import load, outdir, unrle, be16, be32

SD00 = None


def sd00():
    global SD00
    if SD00 is None:
        SD00 = load("SD00")
    return SD00


def packbits(src, size):
    """ByteRun1 as in DISP $111c: n>=0 copy n+1, n=-128 nop, n<0 repeat next byte -n+1 times."""
    out = bytearray()
    i = 0
    while len(out) < size and i < len(src):
        n = src[i]
        i += 1
        if n < 128:
            out += src[i:i + n + 1]
            i += n + 1
        elif n != 128:
            out += bytes([src[i]]) * (257 - n)
            i += 1
    return bytes(out[:size])


def pal12(buf, off, n=32):
    """n words 0x0RGB -> list of (r,g,b) 8-bit."""
    out = []
    for k in range(n):
        v = be16(buf, off + 2 * k)
        out.append((((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17))
    return out


def ehb(pal):
    """Extra Half-Brite: colours 32..63 are colours 0..31 at half brightness."""
    return pal[:32] + [(r >> 1, g >> 1, b >> 1) for (r, g, b) in pal[:32]]


def planar_to_index(data, w, h, depth, interleaved=True, off=0):
    """Return list of rows of pixel indices. Width w in pixels (multiple of 8)."""
    rb = w // 8
    pix = bytearray(w * h)
    for y in range(h):
        for p in range(depth):
            if interleaved:
                base = off + (y * depth + p) * rb
            else:
                base = off + (p * h + y) * rb
            row = data[base:base + rb]
            if len(row) < rb:
                row = row + bytes(rb - len(row))
            bit = 1 << p
            o = y * w
            for xb in range(rb):
                v = row[xb]
                if v:
                    for k in range(8):
                        if v & (0x80 >> k):
                            pix[o + xb * 8 + k] |= bit
    return pix


def save(pix, w, h, pal, path, mask=None):
    img = Image.new("P", (w, h))
    flat = []
    for c in pal[:256]:
        flat += list(c)
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)
    img.putdata(bytes(pix))
    if mask is not None:
        rgba = img.convert("RGBA")
        a = Image.new("L", (w, h))
        a.putdata([255 if m else 0 for m in mask])
        rgba.putalpha(a)
        rgba.save(path)
    else:
        img.save(path)


def us_info(name):
    """Parse a USxx scene file: display struct and palette. Returns dict."""
    b = load(name)
    disp = be16(b, 0)
    f = struct.unpack(">11H", b[disp:disp + 22])
    info = dict(rowbytes=f[2], height=f[3], dw=f[4], dh=f[5], depth=f[8], flags=f[9])
    info["pal"] = pal12(b, disp + f[10], 32)
    # first draw command: cmd 4 'PICx' (USER $214 -> DISP -$4c)
    info["files"] = []
    p = 0x32
    while True:
        fl = be16(b, p)
        nm = b[p + 2:p + 6]
        if nm == b"\0\0\0\0":
            break
        info["files"].append((nm.decode(), fl))
        p += 6
    return info


def export_pics():
    for us, pic in [("US00", "PIC0"), ("US02", "PIC2"), ("US03", "PIC3"), ("US04", "PIC4"),
                    ("US05", "PIC4"), ("US06", "PIC6")]:
        info = us_info(us)
        w = info["rowbytes"] * 8
        h = info["height"]
        d = info["depth"]
        raw = load(pic, packed=False)  # PIC files are not RLE-packed
        bm = packbits(raw, w * h * d // 8)
        pix = planar_to_index(bm, w, h, d)
        od = outdir("png", "PIC")
        fn = os.path.join(od, "%s_%s_%dx%dx%d.png" % (pic, us, w, h, d))
        save(pix, w, h, info["pal"], fn)
        print(fn, info["files"], "flags %x" % info["flags"])


def export_pib():
    s = sd00()
    pal = ehb(pal12(s, 0x21ee + 0x5c, 32))
    for name in ["PIB1", "PIB5"]:
        bm = load(name)
        pix = planar_to_index(bm, 320, 256, 6)
        fn = os.path.join(outdir("png", "PIB"), name + "_320x256x6_ehb.png")
        save(pix, 320, 256, pal, fn)
        print(fn)


if __name__ == "__main__":
    # the exporters above are superseded by gfx_ga.py (right palettes per scene)
    import runpy
    runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "gfx_ga.py"),
                   run_name="__main__")
