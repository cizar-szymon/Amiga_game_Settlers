"""Export the intro sound-effect banks SMP0, SMP2, SMP3, SMP4, SMP5, SML2 to WAV.

Bank layout (after the RLE unpack, big endian), verified against SD00 $41e:
    +0                 u32 total size of the bank (= unpacked file size)
    +4 + 4*(k-1)       u32 offset of sample k (k = 1..N), followed by one more offset = end
    The header ends where the first sample starts (first offset = 4 + 4*(N+1)).
Each sample is a complete IFF 8SVX file (FORM/8SVX, VHDR, NAME, ANNO, BODY).
SD00 plays sample k from offset+$68 (the BODY data, all headers have the same size) to the
next offset, mixed with two other voices at ~7935 Hz (Paula period $1bf on channel 3).

Output: build/intro/wav/<BANK>_<k>_<name>.wav at the VHDR samplesPerSec rate
        and a list of the samples on stdout.
"""
import os
import struct
import wave
from introlib import load, outdir, be16, be32

BANKS = ["SMP0", "SMP2", "SMP3", "SMP4", "SMP5", "SML2"]


def parse_8svx(b):
    assert b[0:4] == b"FORM" and b[8:12] == b"8SVX", b[0:12]
    p = 12
    info = {"name": "", "anno": ""}
    end = 8 + be32(b, 4)
    while p + 8 <= min(end, len(b)):
        cid = b[p:p + 4]
        ln = be32(b, p + 4)
        d = b[p + 8:p + 8 + ln]
        if cid == b"VHDR":
            (info["oneshot"], info["repeat"], info["hicyc"], info["rate"], info["octaves"],
             info["comp"], info["vol"]) = struct.unpack(">IIIHBBI", d[:20])
        elif cid == b"NAME":
            info["name"] = d.split(b"\0")[0].decode("latin-1")
        elif cid == b"ANNO":
            info["anno"] = d.split(b"\0")[0].decode("latin-1")
        elif cid == b"BODY":
            info["body"] = d
            info["body_off"] = p + 8
        p += 8 + ln + (ln & 1)
    return info


def bank(name):
    b = load(name)
    first = be32(b, 4)
    n = (first - 4) // 4 - 1
    offs = [be32(b, 4 + 4 * i) for i in range(n + 1)]
    out = []
    for k in range(n):
        s = b[offs[k]:offs[k + 1]]
        out.append((k + 1, offs[k], offs[k + 1], parse_8svx(s)))
    return b, out


def main():
    d = outdir("wav")
    for name in BANKS:
        b, items = bank(name)
        print("%s: size %d (header says %d), %d samples" % (name, len(b), be32(b, 0), len(items)))
        for k, o0, o1, inf in items:
            body = inf["body"]
            fn = os.path.join(d, "%s_%d_%s.wav" % (name, k, inf["name"].replace(" ", "_") or "x"))
            with wave.open(fn, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(1)
                w.setframerate(inf["rate"])
                w.writeframes(bytes((x + 128) & 0xFF for x in body))  # signed -> unsigned
            print("  %d: %-14s rate %5d Hz, len %6d (%.2f s), oneshot %d repeat %d, vol %x, "
                  "BODY at +%x, anno %r" % (k, inf["name"], inf["rate"], len(body),
                                            len(body) / inf["rate"], inf["oneshot"],
                                            inf["repeat"], inf["vol"], inf["body_off"], inf["anno"]))


if __name__ == "__main__":
    main()
