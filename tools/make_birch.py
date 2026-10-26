"""The birch of "More kinds of trees": 8 frames (the wind, as the trees of
the game) of 32x30 pixels in the colours of the game's trees. Drawn here
(own pixel art after a picture of a birch the user gave: a white trunk
with black marks, an oval crown that lets light through), so it can go
with the game.

  python tools/make_birch.py port/resources [preview.png]

writes birch0.png .. birch7.png (the frames of the wind), transparent
around the tree; then tools/embed_images.py puts them into the program.
The trunk stands where the trunk of the game's tree stands (the game draws
the birch with that tree's offsets).
"""
import math
import random
import sys

from PIL import Image

W, H = 32, 30
FRAMES = 8
TRUNK_X = 15            # the left column of the 2 pixel trunk
CROWN_CX, CROWN_CY = 15.5, 9.5
CROWN_RX, CROWN_RY = 10.5, 9.5
# Light to dark: the colours of the game's deciduous tree.
GREENS = [(119, 187, 68), (102, 153, 0), (68, 136, 0), (51, 102, 51),
          (51, 85, 0), (34, 68, 0)]
BARK_LIGHT, BARK_SHADE, BARK_MARK = (238, 238, 221), (187, 187, 170), (34, 34, 34)


def crown_mask(rng):
    """Leaf clusters inside an oval, with a few holes the light comes
    through (a birch is airy)."""
    pts = {}
    for _ in range(46):
        a = rng.uniform(0, 2 * math.pi)
        d = math.sqrt(rng.uniform(0, 1))
        cx = CROWN_CX + math.cos(a) * d * (CROWN_RX - 2)
        cy = CROWN_CY + math.sin(a) * d * (CROWN_RY - 2)
        r = rng.uniform(1.6, 3.0)
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r and 0 <= x < W and 0 <= y < H:
                    pts[(x, y)] = True
    # Holes.
    for _ in range(7):
        hx = rng.uniform(CROWN_CX - 6, CROWN_CX + 6)
        hy = rng.uniform(CROWN_CY - 3, CROWN_CY + 6)
        for y in range(int(hy) - 1, int(hy) + 2):
            for x in range(int(hx) - 1, int(hx) + 1):
                pts.pop((x, y), None)
    return pts


def shade(x, y, rng):
    """Lit from the upper left, as the game's trees."""
    lx = (x - (CROWN_CX - CROWN_RX)) / (2 * CROWN_RX)
    ly = (y - (CROWN_CY - CROWN_RY)) / (2 * CROWN_RY)
    v = (lx * 0.45 + ly * 0.75) + rng.uniform(-0.18, 0.18)
    i = int(max(0, min(len(GREENS) - 1, v * len(GREENS))))
    return GREENS[i]


def draw(sway, seed=7):
    rng = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    p = img.load()
    # Trunk: from the bottom up to just under the crown (not into it: the
    # crown moves with the wind, a trunk inside showed through its holes
    # now and then - flickering), white, the right column a little shaded,
    # black marks across.
    top = int(CROWN_CY + CROWN_RY) - 3
    marks = {12: 0, 15: 1, 19: 0, 22: 1, 23: 0, 26: 0, 28: 1}
    for y in range(top, H):
        p[TRUNK_X, y] = BARK_LIGHT + (255,)
        p[TRUNK_X + 1, y] = BARK_SHADE + (255,)
        if y in marks:
            p[TRUNK_X + marks[y], y] = BARK_MARK + (255,)
    # Crown, leaning with the wind (upper rows more).
    mask = crown_mask(rng)
    for (x, y) in sorted(mask, key=lambda t: (t[1], t[0])):
        lean = sway if y < CROWN_CY else (sway // 2 if y < CROWN_CY + 5 else 0)
        tx = x + lean
        if 0 <= tx < W:
            p[tx, y] = shade(x, y, rng) + (255,)
    return img


def main():
    out_dir = sys.argv[1]
    sways = [0, 0, 1, 1, 0, 0, -1, -1]
    sheet = Image.new("RGBA", (W * FRAMES, H), (0, 0, 0, 0))
    for i, s in enumerate(sways):
        img = draw(s)
        img.save(f"{out_dir}/birch{i}.png")
        sheet.paste(img, (i * W, 0))
    if len(sys.argv) > 2:
        big = Image.new("RGB", sheet.size, (255, 0, 255))
        big.paste(sheet, (0, 0), sheet)
        big.resize((sheet.width * 4, sheet.height * 4),
                   Image.NEAREST).save(sys.argv[2])


if __name__ == "__main__":
    main()
