"""A blank wooden plate from Freeserf's Advanced button (its edges kept, the
large A and the lettering replaced by the wood of its lower rows), for the
buttons of any width of the MP3 player (port/music-box.cc stretches it:
the left edge, the middle repeated, the right edge).

  python tools/make_button_blank.py freeserf/resources/button-advanced.png \\
                                    port/resources/button-blank.png
"""
import sys

from PIL import Image


def main():
    src, out = sys.argv[1], sys.argv[2]
    orig = Image.open(src).convert("RGB")
    img = orig.copy()
    # Inside the edges (x 1..54, y 1..14) the wood of the lower right part
    # of the button (x 30..41, y 9..14) repeated.
    for y in range(1, 15):
        for x in range(1, 55):
            img.putpixel((x, y), orig.getpixel((30 + x % 12, 9 + (y - 1) % 6)))
    img.save(out)
    print(out, img.size)


if __name__ == "__main__":
    main()
