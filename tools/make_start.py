"""The start picture: a PNG -> IFF ILBM (8 planes, ByteRun1) for the Amiga.

  python tools/make_start.py data/START.png data/start.iff [width] [colors]

The game shows PROGDIR:data/start.iff (or DATA=path/start.iff) while it
starts, scaled to the screen width. Colour 0 is the darkest one: it fills
the rest of the screen.
"""
import struct
import sys

from PIL import Image


def byterun1(row):
    out = bytearray()
    i, n = 0, len(row)
    while i < n:
        j = i
        while j + 1 < n and row[j + 1] == row[i] and j - i < 127:
            j += 1
        if j > i:
            out += bytes([257 - (j - i + 1), row[i]])
            i = j + 1
            continue
        j = i
        while j < n and j - i < 128 and not (j + 2 < n and row[j] == row[j + 1] == row[j + 2]):
            j += 1
        out += bytes([j - i - 1]) + row[i:j]
        i = j
    return out


def chunk(tag, data):
    return tag + struct.pack(">I", len(data)) + data + b"\0" * (len(data) & 1)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 800
    colors = int(sys.argv[4]) if len(sys.argv) > 4 else 128
    im = Image.open(src).convert("RGB")
    h = round(im.height * width / im.width)
    im = im.resize((width, h), Image.LANCZOS)
    q = im.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:colors * 3]
    pal += [0] * (colors * 3 - len(pal))
    order = sorted(range(colors), key=lambda i: sum(pal[i * 3:i * 3 + 3]))
    remap = [0] * colors
    for new, old in enumerate(order):
        remap[old] = new
    px = bytes(remap[p] for p in q.tobytes())
    cmap = bytes(c for old in order for c in pal[old * 3:old * 3 + 3])

    rowbytes = ((width + 15) // 16) * 2
    body = bytearray()
    for y in range(h):
        line = px[y * width:(y + 1) * width]
        for plane in range(8):
            bits = bytearray(rowbytes)
            for x, v in enumerate(line):
                if v >> plane & 1:
                    bits[x >> 3] |= 0x80 >> (x & 7)
            body += byterun1(bytes(bits))

    bmhd = struct.pack(">HHhhBBBBHBBhh", width, h, 0, 0, 8, 0, 1, 0, 0, 10, 11, width, h)
    form = b"ILBM" + chunk(b"BMHD", bmhd) + chunk(b"CMAP", cmap) + chunk(b"BODY", bytes(body))
    with open(dst, "wb") as f:
        f.write(b"FORM" + struct.pack(">I", len(form)) + form)
    print("%s: %dx%d, %d colours, %d bytes" % (dst, width, h, colors, 8 + len(form)))


if __name__ == "__main__":
    main()
