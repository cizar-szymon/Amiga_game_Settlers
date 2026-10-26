"""The picture of the Settlers icon: the logo of the start picture, smaller,
on a black field with "RTG" in the top right corner. The icon is then made
from it with make_icon.py (see docs/PORTING.md).

  python tools/make_icon_picture.py data/START.png OUT.png [W H LOGO_W]
"""
import sys

from PIL import Image

# 3x5 letters of the RTG mark
GLYPHS = {
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "G": [".##", "#..", "#.#", "#.#", ".##"],
}
GOLD = (238, 187, 51)
SHADOW = (85, 34, 0)


def text(img, x, y, s):
    for pass_, colour, off in ((0, SHADOW, 1), (1, GOLD, 0)):
        cx = x
        for ch in s:
            for r, row in enumerate(GLYPHS[ch]):
                for c, v in enumerate(row):
                    if v == "#":
                        img.putpixel((cx + c + off, y + r + off), colour)
            cx += 4


def main():
    src, out = sys.argv[1], sys.argv[2]
    w, h, logo_w = (int(v) for v in (sys.argv[3:6] or (120, 56, 96)))
    logo = Image.open(src).convert("RGB").crop((39, 150, 39 + 1292, 150 + 404))
    logo_h = round(logo.height * logo_w / logo.width)
    logo = logo.resize((logo_w, logo_h), Image.LANCZOS)
    img = Image.new("RGB", (w, h), (0, 0, 0))
    img.paste(logo, ((w - logo_w) // 2, (h - logo_h) // 2 + 2))
    text(img, w - 3 * 4 - 3, 7, "RTG")
    img.save(out)
    print("%s: %dx%d, logo %dx%d" % (out, w, h, logo_w, logo_h))


if __name__ == "__main__":
    main()
