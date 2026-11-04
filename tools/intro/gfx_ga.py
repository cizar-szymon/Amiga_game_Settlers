"""Second half of the intro graphics export: PIC/PIB/KPIC with the right palettes, GA archives.

usage: python gfx_ga.py        (imports the decoders from gfx.py)
"""
import os
from PIL import Image
from introlib import load, outdir, be32
from gfx import packbits, pal12, ehb, planar_to_index, save, us_info, sd00

PIC_SCENES = [("US00", "PIC0"), ("US02", "PIC2"), ("US03", "PIC3"), ("US04", "PIC4"),
              ("US06", "PIC6")]  # US05 names PIC5/GA05, which are not on the disks


def pic_palette(us):
    if us == "US06":  # US06 palette is all black; SD00 fades in its own table at $6762
        return pal12(sd00(), 0x6762, 32)
    return us_info(us)["pal"]


def pic_bitmap(us, pic):
    info = us_info(us)
    w, h, d = info["rowbytes"] * 8, info["height"], info["depth"]
    raw = load(pic, packed=False)  # PIC files are NOT RLE-packed, only ByteRun1
    return packbits(raw, w * h * d // 8), w, h, d


def export_pics():
    od = outdir("png", "PIC")
    for us, pic in PIC_SCENES:
        bm, w, h, d = pic_bitmap(us, pic)
        pix = planar_to_index(bm, w, h, d)
        fn = os.path.join(od, "%s_%dx%dx%d.png" % (pic, w, h, d))
        save(pix, w, h, pic_palette(us), fn)
        print(fn)


def export_pib():
    s = sd00()
    od = outdir("png", "PIB")
    for name, paloff in [("PIB1", 0x21ee + 0x5c), ("PIB5", 0x3cc6 + 0x2e)]:
        pal = ehb(pal12(s, paloff, 32))
        pix = planar_to_index(load(name), 320, 256, 6)
        fn = os.path.join(od, name + "_320x256x6_ehb.png")
        save(pix, 320, 256, pal, fn)
        print(fn)


def export_kpic():
    """KPIC: 208x126x5 interleaved (SD00 struct $5fec), KMSK: 208x126x1 mask (struct $6048)."""
    pal = us_info("US03")["pal"]
    pix = planar_to_index(load("KPIC"), 208, 126, 5)
    mask = planar_to_index(load("KMSK"), 208, 126, 1)
    od = outdir("png", "KPIC")
    save(pix, 208, 126, pal, os.path.join(od, "KPIC_208x126x5.png"))
    save(pix, 208, 126, pal, os.path.join(od, "KPIC_masked.png"), mask)
    save(mask, 208, 126, [(0, 0, 0), (255, 255, 255)], os.path.join(od, "KMSK_208x126x1.png"))
    print(od)


# ---- GA archives -------------------------------------------------------------------------

GA_INFO = {  # name: (depth, palette)
    "GA00": (5, lambda: us_info("US00")["pal"]),
    "GA01": (6, lambda: ehb(pal12(sd00(), 0x21ee + 0x5c, 32))),
    "GA02": (5, lambda: us_info("US02")["pal"]),
    "GA03": (5, lambda: us_info("US03")["pal"]),
    "GA04": (5, lambda: us_info("US04")["pal"]),
    "GAB2": (5, lambda: pal12(sd00(), 0x5b14 + 0x5c, 32)),
    "GAC2": (5, lambda: pal12(sd00(), 0x5b14 + 0x5c, 32)),
}
# widths (bytes) confirmed from SD00 blit calls where the smoothness guess fails (noisy textures)
WIDTH_OVERRIDE = {("GA01", i): 4 for i in range(24)}  # SD00 $1c16..$1ca8: 25x25, 18x8, 26x36
# GA01 entries 34..45 are 5-plane image/mask pairs (80x150 knight seen from behind) used in the
# gate scene (SD00 $35ee, PIB5, palette at SD00 $3cf4); the rest of GA01 is 6-plane (EHB, $1a28).
GA_SECTION = {"GA01": {i: (5, lambda: ehb(pal12(sd00(), 0x3cc6 + 0x2e, 32))) for i in range(34, 46)}}
DELTA_BASE = {"GA03": ("US03", "PIC3"), "GA04": ("US04", "PIC4")}


def ga_entries(b):
    first = be32(b, 4)
    n = (first - 4) // 4
    offs = [be32(b, 4 + 4 * i) for i in range(n)] + [len(b)]
    return [b[offs[i]:offs[i + 1]] for i in range(n)]


def is_delta(e):
    """Delta frame: records (long screen offset, 5 longs = 32 pixels of planes 0..4), -1 = end."""
    if len(e) < 28 or (len(e) - 4) % 24:
        return False
    return be32(e, len(e) - 4) == 0xFFFFFFFF and be32(e, 0) < 0x19000


def apply_delta(bm, e, plane=40):
    """SD00 $6076: for each record write 5 longs at offset, +40, +80, +120, +160."""
    bm = bytearray(bm)
    p = 0
    while True:
        o = be32(e, p)
        if o & 0x80000000:
            break
        for k in range(5):
            bm[o + k * plane:o + k * plane + 4] = e[p + 4 + 4 * k:p + 8 + 4 * k]
        p += 24
    return bytes(bm)


def roughness(pix, w, h):
    diff = 0
    for y in range(1, h):
        a = pix[(y - 1) * w:y * w]
        c = pix[y * w:(y + 1) * w]
        diff += sum(1 for i in range(w) if a[i] != c[i])
    return diff / float(w * (h - 1))


def guess_width(e, depth, msize=None):
    """Width in bytes (even); h = len/(wb*depth); pick the smoothest. A mask of wb*h bytes pins it."""
    best = None
    for wb in range(2, 62, 2):
        h = len(e) // (wb * depth)
        if h < 2 or len(e) - wb * h * depth >= wb * depth:
            continue
        if msize is not None and abs(msize - wb * h) > 4:
            continue
        r = roughness(planar_to_index(e, wb * 8, h, depth), wb * 8, h)
        if best is None or r < best[0]:
            best = (r, wb, h)
    return best


def classify(ents, depth):
    """Kinds per entry: 'delta', 'image', 'mask'; maskof[i] = index of the mask of image i."""
    n = len(ents)
    kind = ["delta" if is_delta(e) else "image" for e in ents]
    maskof = [None] * n

    def fits(i, j):
        return kind[i] == "image" and kind[j] == "image" and \
            abs(len(ents[i]) // depth - len(ents[j])) <= 4 and len(ents[j]) >= 8

    # block layout first: a run of equal-size images followed by a run of masks (GA04)
    i = 0
    while i < n:
        j = i
        while j < n and kind[j] == "image" and len(ents[j]) == len(ents[i]):
            j += 1
        run = j - i
        if run > 1 and j + run <= n and all(fits(i + k, j + k) for k in range(run)) and \
                not fits(i, i + 1):
            for k in range(run):
                kind[j + k] = "mask"
                maskof[i + k] = j + k
            i = j + run
        else:
            i = max(j, i + 1)
    # alternating image, mask pairs (GA00 tail, GA01, GA02, GAB2, GAC2)
    i = 0
    while i < n - 1:
        if maskof[i] is None and fits(i, i + 1):
            kind[i + 1] = "mask"
            maskof[i] = i + 1
            i += 2
        else:
            i += 1
    return kind, maskof


def sheet(images, path, bg=(64, 64, 64)):
    if not images:
        return
    cols = min(8, len(images))
    cw = max(im.width for im in images) + 4
    ch = max(im.height for im in images) + 4
    rows = (len(images) + cols - 1) // cols
    s = Image.new("RGB", (cw * cols, ch * rows), bg)
    for k, im in enumerate(images):
        rgba = im.convert("RGBA")
        s.paste(rgba, ((k % cols) * cw + 2, (k // cols) * ch + 2), rgba)
    s.save(path)


def export_ga(name):
    depth, palf = GA_INFO[name]
    pal = palf()
    ents = ga_entries(load(name))
    kind, maskof = classify(ents, depth)
    pal_main, depth_main = pal, depth
    for i, (d, pf) in GA_SECTION.get(name, {}).items():  # sections drawn in another scene
        kind[i] = "image" if i % 2 == 0 else "mask"
        maskof[i] = i + 1 if i % 2 == 0 else None
    od = outdir("png", name)
    lines, imgs, frames = [], [], []
    base = pic_bitmap(*DELTA_BASE[name])[0] if name in DELTA_BASE else None
    for i, e in enumerate(ents):
        sec = GA_SECTION.get(name, {}).get(i)
        depth, pal = (sec[0], sec[1]()) if sec else (depth_main, pal_main)
        if kind[i] == "delta":
            base = apply_delta(base, e)
            pix = planar_to_index(base, 320, 256, 5)
            fn = os.path.join(od, "%s_%02d_delta.png" % (name, i))
            save(pix, 320, 256, pal, fn)
            frames.append(Image.open(fn))
            lines.append("%2d delta  %6d bytes  %d records" % (i, len(e), (len(e) - 4) // 24))
        elif kind[i] == "image":
            m = maskof[i]
            if (name, i) in WIDTH_OVERRIDE:
                wb = WIDTH_OVERRIDE[(name, i)]
                g = (0.0, wb, len(e) // (wb * depth))
            else:
                g = guess_width(e, depth, len(ents[m]) if m is not None else None) or \
                    guess_width(e, depth)
            if g is None:
                lines.append("%2d image  %6d bytes  size unknown" % (i, len(e)))
                continue
            r, wb, h = g
            w = wb * 8
            pix = planar_to_index(e, w, h, depth)
            mask = planar_to_index(ents[m], w, h, 1) if m is not None else None
            fn = os.path.join(od, "%s_%02d_%dx%d.png" % (name, i, w, h))
            save(pix, w, h, pal, fn, mask)
            imgs.append(Image.open(fn))
            lines.append("%2d image  %6d bytes  %3dx%-3d x%d  mask=%s  rough=%.2f" %
                         (i, len(e), w, h, depth, m if m is not None else "-", r))
        else:
            lines.append("%2d mask   %6d bytes" % (i, len(e)))
    with open(os.path.join(od, "index.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    sheet(imgs, os.path.join(outdir("png"), "sheet_%s.png" % name))
    if frames:
        sheet([fr.convert("RGB").resize((160, 128)) for fr in frames],
              os.path.join(outdir("png"), "sheet_%s_delta.png" % name))
    print(name + "\n  " + "\n  ".join(lines))


if __name__ == "__main__":
    export_pics()
    export_pib()
    export_kpic()
    for n in GA_INFO:
        export_ga(n)
