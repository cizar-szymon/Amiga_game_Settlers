"""The "Amiga" button of the options box (Amiga port): Freeserf's Advanced
button with its large A kept and MIGA in the lettering of the original (5
pixel high yellow letters on the dark band).

  python tools/make_button_amiga.py freeserf/resources/button-advanced.png \\
                                    port/resources/button-amiga.png
"""
import sys

from PIL import Image

GLYPHS = {
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "G": [".####", "#....", "#..##", "#...#", ".####"],
    "A": ["#####", "#...#", "#####", "#...#", "#...#"],
}
DARK = (67, 35, 19)
YELLOW = (255, 255, 0)


def main():
    src, out = sys.argv[1], sys.argv[2]
    img = Image.open(src).convert("RGB")
    orig = img.copy()
    word = "MIGA"
    width = sum(len(GLYPHS[c][0]) for c in word) + len(word) - 1
    # The small letters start right after the large A (x 12), on a dark band
    # one pixel wider than them (y 2..8); right of it the wood of the button,
    # taken from its lower rows (y 9..14).
    end = 12 + width + 1
    for y in range(2, 9):
        for x in range(11, 54):
            if x <= end:
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
    # Only as long as its word: the left part up to the band, then the right
    # edge of the frame of the original button (its last two columns).
    cut = end + 2
    short = Image.new("RGB", (cut + 2, img.height))
    short.paste(img.crop((0, 0, cut, img.height)), (0, 0))
    short.paste(orig.crop((54, 0, 56, img.height)), (cut, 0))
    short.save(out)
    print(out)


if __name__ == "__main__":
    main()
