"""The "Video" and "Audio" buttons of the Options box (Amiga port), in the
style of Freeserf's Advanced button: a large first letter, the rest of the
word in the 5 pixel high yellow letters of the original on the dark band,
as long as its word (as tools/make_button_amiga.py).

  python tools/make_button_word.py freeserf/resources/button-advanced.png port/resources
"""
import os
import sys

from PIL import Image

GLYPHS = {
    "D": ["####.", ".#..#", ".#..#", ".#..#", "####."],
    "E": ["#####", ".#..#", ".##..", ".#..#", "#####"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "O": [".###.", "#...#", "#...#", "#...#", ".###."],
    "U": ["#...#", "#...#", "#...#", "#...#", ".###."],
}
# The large V, in the cell of the large A of the button (x 1..12, y 2..13):
# its yellow pixels, the dark outline drawn around them.
BIG_V_YELLOW = {
    3: [2, 3, 4, 5, 7, 8, 9, 10],
    4: [3, 4, 8, 9], 5: [3, 4, 8, 9], 6: [3, 4, 8, 9], 7: [3, 4, 8, 9],
    8: [3, 4, 8, 9], 9: [3, 4, 8, 9],
    10: [4, 5, 7, 8],
    11: [5, 6, 7],
    12: [6],
}
DARK = (67, 35, 19)
YELLOW = (255, 255, 0)


def wood(orig, x, y):
    """The wood of the button under the band (its lower rows, right part)."""
    return orig.getpixel((30 + x % 12, 9 + (y - 2) % 6))


def make(orig, word, big_v, out):
    img = orig.copy()
    if big_v:
        # The large A away: wood, then the V with its outline.
        for y in range(2, 14):
            for x in range(1, 13):
                img.putpixel((x, y), wood(orig, x, y))
        for y, xs in BIG_V_YELLOW.items():
            for x in xs:
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        img.putpixel((x + dx, y + dy), DARK)
        for y, xs in BIG_V_YELLOW.items():
            for x in xs:
                img.putpixel((x, y), YELLOW)
    width = sum(len(GLYPHS[c][0]) for c in word) + len(word) - 1
    end = 12 + width + 1
    for y in range(2, 9):
        for x in range(11, 54):
            if x <= end:
                if not (big_v and x == 11 and img.getpixel((x, y)) == YELLOW):
                    img.putpixel((x, y), DARK)
            else:
                img.putpixel((x, y), orig.getpixel((x, 9 + (y - 2) % 6)))
    x = 12
    for c in word:
        for r, row in enumerate(GLYPHS[c]):
            for col, v in enumerate(row):
                if v == "#":
                    img.putpixel((x + col, 3 + r), YELLOW)
        x += len(GLYPHS[c][0]) + 1
    cut = end + 2
    short = Image.new("RGB", (cut + 2, img.height))
    short.paste(img.crop((0, 0, cut, img.height)), (0, 0))
    short.paste(orig.crop((54, 0, 56, img.height)), (cut, 0))
    short.save(out)
    print(out, short.size)


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    orig = Image.open(src).convert("RGB")
    make(orig, "IDEO", True, os.path.join(outdir, "button-video.png"))
    make(orig, "UDIO", False, os.path.join(outdir, "button-audio.png"))


if __name__ == "__main__":
    main()
