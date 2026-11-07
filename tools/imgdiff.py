"""Compare two screenshots (PPM) pixel by pixel, the FPS counter at the top
left left out; a picture of the differences (red) when there are any.
usage: python tools/imgdiff.py a.ppm b.ppm diff.png label"""
import sys
from PIL import Image, ImageChops

a_p, b_p, out, label = sys.argv[1:5]
try:
    a = Image.open(a_p).convert('RGB')
    b = Image.open(b_p).convert('RGB')
except Exception as e:  # a missing screenshot
    print('%-28s NO SCREENSHOT (%s)' % (label, e))
    sys.exit(0)
if a.size != b.size:
    print('%-28s SIZE %s vs %s' % (label, a.size, b.size))
    sys.exit(0)
d = ImageChops.difference(a, b)
px = d.load()
n = 0
box = None
for y in range(a.size[1]):
    for x in range(a.size[0]):
        if px[x, y] != (0, 0, 0) and not (y < 14 and x < 80):
            n += 1
            box = (x, y) if box is None else box
if n == 0:
    print('%-28s %s identical' % (label, a.size))
else:
    m = d.convert('L').point(lambda v: 255 if v else 0)
    base = a.convert('L').point(lambda v: v // 3).convert('RGB')
    base.paste(Image.new('RGB', a.size, (255, 0, 0)), mask=m)
    base.save(out)
    print('%-28s %s DIFFERENT: %d pixels, first at %s' % (label, a.size, n,
                                                          box))
