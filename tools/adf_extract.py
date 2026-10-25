"""List and extract files from an AmigaDOS OFS/FFS ADF image.
usage: adf_extract.py IMAGE [OUTDIR]   (without OUTDIR only lists)"""
import os
import struct
import sys

BS = 512


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def i32(b, o):
    return struct.unpack_from(">i", b, o)[0]


def main():
    img = open(sys.argv[1], "rb").read()
    out = sys.argv[2] if len(sys.argv) > 2 else None
    ffs = img[3] & 1
    blk = lambda n: img[n * BS:(n + 1) * BS]
    root = len(img) // BS // 2
    rb = blk(root)
    print("volume:", rb[BS - 79:BS - 79 + rb[BS - 80]].decode("latin-1"), "FFS" if ffs else "OFS")

    def name(b):
        return b[BS - 79:BS - 79 + b[BS - 80]].decode("latin-1")

    def read_file(hb):
        size = u32(hb, BS - 188)
        data = bytearray()
        b = hb
        while True:
            cnt = u32(b, 8)
            ptrs = [u32(b, 24 + 4 * (71 - i)) for i in range(cnt)]
            for p in ptrs:
                d = blk(p)
                data += d if ffs else d[24:24 + u32(d, 12)]
            ext = u32(b, BS - 8)
            if not ext:
                break
            b = blk(ext)
        return bytes(data[:size])

    def walk(b, path):
        for i in range(72):
            n = u32(b, 24 + 4 * i)
            while n:
                hb = blk(n)
                sec = i32(hb, BS - 4)
                p = path + "/" + name(hb) if path else name(hb)
                if sec == 2:
                    print("%-40s <DIR>" % p)
                    if out:
                        os.makedirs(os.path.join(out, p), exist_ok=True)
                    walk(hb, p)
                elif sec == -3:
                    data = read_file(hb)
                    print("%-40s %8d" % (p, len(data)))
                    if out:
                        os.makedirs(os.path.dirname(os.path.join(out, p)) or out, exist_ok=True)
                        open(os.path.join(out, p), "wb").write(data)
                n = u32(hb, BS - 16)

    walk(rb, "")


if __name__ == "__main__":
    main()
