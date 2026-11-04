"""Compare the C++ intro player (port/introtest.cc) with the Python reference player_ref.py.

  python check_player.py OUTDIR
OUTDIR is where introtest wrote crc.txt and sfx.raw. The reference: the frames in
build/intro/frames (player_ref.py, identical to the emulator) and build/intro/sfx_ref.raw.
"""
import os
import sys
import zlib
import numpy as np
from PIL import Image
from introlib import OUT


def main():
    out = sys.argv[1]
    got = {}
    for line in open(os.path.join(out, "crc.txt")):
        f, c = line.split()
        got[int(f)] = int(c, 16)
    bad = []
    for f in sorted(got):
        rgb = np.array(Image.open(os.path.join(OUT, "frames", "%05d.png" % f)).convert("RGB"))
        if zlib.crc32(rgb.tobytes()) != got[f]:
            bad.append(f)
    print("frames compared: %d, different: %d %s" % (len(got), len(bad), bad[:20]))
    sfx = np.fromfile(os.path.join(out, "sfx.raw"), np.int8)
    ref = np.fromfile(os.path.join(OUT, "sfx_ref.raw"), np.int8)[:sfx.size]
    diff = np.nonzero(ref != sfx[:ref.size])[0]
    print("effect samples compared: %d, different: %d %s" % (ref.size, diff.size,
                                                            sorted(set(diff // 160))[:20]))


if __name__ == "__main__":
    main()
