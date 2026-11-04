"""Reference player for the Settlers Amiga intro (Python model of the C++ player for our port).

Input (data-driven, nothing of the original code is executed here):
  - the original data files from data/disk1 + data/disk2 (RLE / ByteRun1 / IFF 8SVX / MOD),
  - the intro script build/intro/emu/timeline.json + views.json, recorded once by emu.py from
    the original SD00 code (see docs/INTRO.md, "Skrypt intro"):
      ops   per frame (50 Hz): alloc, load, fill, copy, poke (CPU writes: copy runs and
            literal bytes), unpack (ByteRun1), delta (GA03/GA04 frame differences), blit (Amiga blitter registers, pointers as [block, offset]),
            sfx / sfxbank / sfx_on / sfx_off / sfx_stop (SD00 3-voice mixer), music, scene
      views per frame: display bands [first line, plane pointers, depth, BPLCON1, DDFSTRT,
            BPL1MOD, BPL2MOD, 32 colours, DIWSTRT, DIWSTOP, DDFSTOP]
The player keeps the Amiga bitmaps in planar form (blocks), runs the recorded blits with a
blitter model, converts the shown bitmap to pixels per frame (c2p) and mixes the sound.

Output (build/intro/):
  frames/NNNNN.png    320x256 per frame (with --png, default on)
  intro_ref.wav       music (ProTracker, 3 voices) + effects (channel 3), 22050 Hz stereo
  intro_ref.mp4       if ffmpeg is found (video 640x512, nearest-neighbour x2)
usage: python player_ref.py [--check] [--no-png] [--frames N]
  --check   compare every frame with the emulator frames (build/intro/emu/frames) and the
            effect mixer output with build/intro/emu/sfx.raw
"""
import json
import os
import shutil
import subprocess
import sys
import wave
import numpy as np
from PIL import Image
from introlib import load, outdir, OUT


def be32(b, o):
    """Big-endian long from bytes or a numpy uint8 array (no uint8 overflow)."""
    return (int(b[o]) << 24) | (int(b[o + 1]) << 16) | (int(b[o + 2]) << 8) | int(b[o + 3])

PAL_CLOCK = 3546895
SFX_RATE = 7935            # Paula period $1bf, 160 bytes per buffer
OUT_RATE = 22050
MUSIC_FADE_FRAMES = 65     # SOUN -$3c: 65 volume steps, one per frame


def file_bytes(name):
    return load(name, packed=not name.startswith("PIC"))


def byterun1(src, n):
    out = bytearray()
    i = 0
    while len(out) < n:
        c = src[i]
        i += 1
        if c < 128:
            out += src[i:i + c + 1]
            i += c + 1
        elif c > 128:
            out += bytes([src[i]]) * (257 - c)
            i += 1
    return bytes(out[:n])


PAD = 64                   # guard bytes before/after every block (blits may start one word early)


def fix_pointers(tl):
    """Old recordings: pointers just before a block were left absolute -> [block, -n]."""
    starts = sorted((a, i) for i, (a, size, mod) in enumerate(tl["blocks"]) if not mod)

    def fix(p):
        if isinstance(p, list) and len(p) == 2 and p[0] == "abs":
            for a, i in starts:
                if 0 < a - p[1] <= PAD:
                    return [i, p[1] - a]
            raise ValueError("pointer %x outside all blocks" % p[1])
        return p
    for o in tl["ops"]:
        if o[1] == "blit":
            o[8] = [fix(p) if p is not None else None for p in o[8]]
        else:
            for k in range(2, len(o)):
                o[k] = fix(o[k])


class Memory:
    """Data blocks as numpy byte arrays; offset 0 of a block is at index PAD."""

    def __init__(self):
        self.blocks = {}
        self.mods = {}

    def src(self, p):
        """Block array + offset; ["mod", NAME, off] = static data inside a code module file."""
        if p[0] == "mod":
            if p[1] not in self.mods:
                self.mods[p[1]] = np.frombuffer(load(p[1]) + bytes(0x1000), np.uint8).copy()
            return self.mods[p[1]], p[2]
        return self.blocks[p[0]], p[1] + PAD

    def alloc(self, b, size):
        self.blocks[b] = np.zeros(size + 2 * PAD, np.uint8)

    def view(self, p, n):
        b, off = p
        return self.blocks[b][off + PAD:off + PAD + n]

    def write(self, p, data):
        b, off = p
        self.blocks[b][off + PAD:off + PAD + len(data)] = np.frombuffer(bytes(data), np.uint8)


def blit(mem, con0, con1, size, fwm, lwm, mods, ptrs, dat):
    """Amiga blitter, area mode: D = f(A, B, C) with shifts, masks, ascending/descending."""
    h = (size >> 6) or 1024
    w = (size & 0x3F) or 64
    desc = bool(con1 & 2)
    ash, bsh = con0 >> 12, con1 >> 12
    mint = con0 & 0xFF
    use = [(con0 >> (11 - i)) & 1 for i in range(4)]
    n = w * h
    sgn = -1 if desc else 1

    def offs(ch):
        col = np.arange(w, dtype=np.int64) * 2 * sgn
        row = np.arange(h, dtype=np.int64) * (2 * w + mods[ch]) * sgn
        return (ptrs[ch][1] + PAD + row[:, None] + col[None, :]).ravel()

    def fetch(ch):
        arr = mem.blocks[ptrs[ch][0]]
        o = offs(ch)
        return (arr[o].astype(np.uint32) << 8) | arr[o + 1]

    A = fetch(0) if use[0] else np.full(n, dat[0], np.uint32)
    B = fetch(1) if use[1] else np.full(n, dat[1], np.uint32)
    C = fetch(2) if use[2] else np.full(n, dat[2], np.uint32)
    col = np.tile(np.arange(w), h)
    A = A.copy()
    A[col == 0] &= fwm
    A[col == w - 1] &= lwm

    def shift(x, s):
        if not s:
            return x
        prev = np.concatenate(([0], x[:-1])).astype(np.uint32)
        if desc:
            return ((x << s) | (prev >> (16 - s))) & 0xFFFF
        return (((prev << 16) | x) >> s) & 0xFFFF

    As, Bs = shift(A, ash), shift(B, bsh)
    nA, nB, nC = ~As & 0xFFFF, ~Bs & 0xFFFF, ~C & 0xFFFF
    terms = [nA & nB & nC, nA & nB & C, nA & Bs & nC, nA & Bs & C,
             As & nB & nC, As & nB & C, As & Bs & nC, As & Bs & C]
    D = np.zeros(n, np.uint32)
    for i in range(8):
        if mint & (1 << i):
            D |= terms[i]
    if use[3]:
        arr = mem.blocks[ptrs[3][0]]
        o = offs(3)
        arr[o] = (D >> 8) & 0xFF
        arr[o + 1] = D & 0xFF


def apply_delta(mem, src, dst):
    """GA03/GA04 frame differences (SD00 $6074): offset, then 5 longs at +0/+40/+80/+120/+160."""
    s = mem.blocks[src[0]]
    p = src[1] + PAD
    d = mem.blocks[dst[0]]
    while True:
        off = int(be32(s, p))
        p += 4
        if off & 0x80000000:
            break
        base = dst[1] + PAD + off
        for k in range(5):
            d[base + 40 * k: base + 40 * k + 4] = s[p:p + 4]
            p += 4


def render(mem, bands):
    """Bands of the PAL window (from the copper) -> 320x256 RGB."""
    rgb = np.zeros((256, 320, 3), np.uint8)
    xs = np.arange(320)
    for bi, band in enumerate(bands):
        oy, planes, depth, con1, ddfs, m1, m2, pal32, dstrt, dstop = band[:10]
        ddfe = band[10] if len(band) > 10 else 0xD0
        y1 = bands[bi + 1][0] if bi + 1 < len(bands) else 256
        cols = np.array([[((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17]
                         for v in pal32], np.uint8)
        cols = np.concatenate([cols, cols // 2])
        nwords = (ddfe - ddfs) // 8 + 1
        ptr = [list(p) for p in planes]
        hs = (dstrt & 0xFF) - 0x81
        he = (dstop & 0xFF) + 0x100 - 0x81
        x0 = ddfs * 2 + 17 - 0x81
        i = xs - x0
        for y in range(oy, y1):
            pix = np.zeros(nwords * 16 + 16, np.uint8)
            for k in range(depth):
                raw = mem.view(ptr[k], nwords * 2)
                bits = np.unpackbits(raw)
                delay = (con1 >> (4 if k & 1 else 0)) & 0xF
                pix[delay:delay + bits.size] |= (bits << k).astype(np.uint8)
                ptr[k][1] += nwords * 2 + (m2 if k & 1 else m1)
            if depth < 6:
                pix &= 31
            ok = (i >= 0) & (i < pix.size) & (xs >= hs) & (xs < he)
            rgb[y] = cols[np.where(ok, pix[np.clip(i, 0, pix.size - 1)], 0)]
    return rgb


class SfxMixer:
    """SD00 $41e (start) and $558 (mix): 3 voices into one 8-bit stream for Paula channel 3."""

    def __init__(self):
        tab = np.zeros((64, 256), np.int16)
        for vol in range(64):
            for b in range(256):
                s = b - 256 if b > 127 else b
                v = (s * vol * 1024) >> 16              # muls, add, swap: floor(s*vol/64)
                q = abs(v) // 3
                tab[vol, b] = q if v >= 0 else -q      # divs #3 truncates toward zero
        self.tab = tab
        self.reset()
        self.bank = None

    def reset(self):
        self.v = [dict(ptr=0, end=0, start=0, vol=0, cnt=0, data=None) for _ in range(3)]

    def start(self, mem, k, vol, rep):
        b, off = self.bank
        arr = mem.blocks[b]
        off += PAD
        s = off + int(be32(arr, off + 4 * k)) + 0x68
        e = off + int(be32(arr, off + 4 * k + 4))
        for v in self.v:
            if v["ptr"] == v["end"]:
                v.update(ptr=s, end=e, start=s, vol=vol & 0x3F, cnt=(rep - 1) & 0xFFFF, data=arr)
                return

    def mix(self, n=160):
        out = np.zeros(n, np.int16)
        for vi, v in enumerate(self.v):
            if v["data"] is None:
                continue
            for i in range(n):
                if v["ptr"] == v["end"]:
                    if v["cnt"] == 0:
                        break
                    v["cnt"] -= 1
                    v["ptr"] = v["start"]
                out[i] += self.tab[v["vol"], v["data"][v["ptr"]]]
                v["ptr"] += 1
        return ((out + 128) & 0xFF).astype(np.uint8) - 128     # byte adds wrap


def music_track(frames_total, start_frame, fade_frame):
    import music
    mod = music.parse(load("MUSI"))
    seconds = (frames_total - start_frame) / 50.0 + 1
    loops = int(seconds // 115.2) + 1
    pcm, left, right = music.render(mod, loops=loops)
    n = int(frames_total * OUT_RATE / 50)
    L = np.zeros(n, np.float32)
    Rr = np.zeros(n, np.float32)
    s0 = int(start_frame * OUT_RATE / 50)
    m = min(n - s0, left.size)
    L[s0:s0 + m] = left[:m]
    Rr[s0:s0 + m] = right[:m]
    if fade_frame is not None:
        f0 = int(fade_frame * OUT_RATE / 50)
        f1 = int((fade_frame + MUSIC_FADE_FRAMES) * OUT_RATE / 50)
        ramp = np.ones(n, np.float32)
        ramp[f0:f1] = np.linspace(1, 0, max(f1 - f0, 1))[:max(0, min(f1, n) - f0)]
        ramp[f1:] = 0
        L *= ramp
        Rr *= ramp
    return L, Rr


def main():
    check = "--check" in sys.argv
    png = "--no-png" not in sys.argv
    emu_dir = os.path.join(OUT, sys.argv[sys.argv.index("--emu") + 1] if "--emu" in sys.argv else "emu")
    tl = json.load(open(os.path.join(emu_dir, "timeline.json")))
    views = json.load(open(os.path.join(emu_dir, "views.json")))
    nframes = len(views)
    if "--frames" in sys.argv:
        nframes = min(nframes, int(sys.argv[sys.argv.index("--frames") + 1]))
    fix_pointers(tl)
    ops = tl["ops"]
    mem = Memory()
    sfx = SfxMixer()
    sfx_on = False
    sfx_stream = []
    music_start = music_fade = None
    fdir = outdir("frames")
    mism = []
    ref_sfx = open(os.path.join(emu_dir, "sfx.raw"), "rb").read() if check else None
    oi = 0
    # the emulator records the buffer Paula plays during the frame; it was mixed by the
    # interrupt at the start of the same frame (no extra latency in this model)
    for f in range(nframes):
        # the channel-3 interrupt mixes the next buffer at the start of the frame, before the
        # scene code of this frame runs (SD00 $50a/$558)
        for phase in (0, 1):
            if phase == 1:
                buf = sfx.mix() if sfx_on else np.zeros(160, np.int16)
                sfx_stream.append(buf.astype(np.int8))
            limit = f - 0.25 if phase == 0 else f     # interrupt ops (f - 0.5) before the mix
            while oi < len(ops) and ops[oi][0] <= limit:
                o = ops[oi]
                oi += 1
                k = o[1]
                if k == "alloc":
                    mem.alloc(o[2], o[3])
                elif k == "load":
                    mem.write(o[3], file_bytes(o[2])[:o[4]])
                elif k == "fill":
                    mem.write(o[2], bytes([o[4]]) * o[3])
                elif k == "copy":
                    arr, off = mem.src(o[2])
                    mem.write(o[3], arr[off:off + o[4]].tobytes())
                elif k == "poke":
                    mem.write(o[2], bytes.fromhex(o[3]))
                elif k == "unpack":
                    b, off = o[2]
                    mem.write(o[3], byterun1(mem.blocks[b][off + PAD:].tobytes(), o[4]))
                elif k == "delta":
                    apply_delta(mem, o[2], o[3])
                elif k == "blit":
                    con0, con1, size, fwm, lwm, mods, ptrs, dat = o[2:10]
                    blit(mem, con0, con1, size, fwm, lwm, mods, ptrs, dat)
                elif k == "sfxbank":
                    sfx.bank = o[2]
                elif k == "sfx":
                    sfx.start(mem, o[2], o[3], o[4])
                elif k == "sfx_stop":
                    sfx.v[o[2]]["vol"] = 0
                    sfx.v[o[2]]["cnt"] = 0
                elif k == "sfx_on":
                    sfx.reset()
                    sfx_on = True
                elif k == "sfx_off":
                    sfx_on = False
                elif k == "music":
                    if o[2] == "play" and music_start is None:
                        music_start = f
                    elif o[2] == "fade" and o[3] & 0x80000000:
                        music_fade = f
        rgb = render(mem, views[f]) if views[f] else np.zeros((256, 320, 3), np.uint8)
        if png:
            Image.fromarray(rgb).save(os.path.join(fdir, "%05d.png" % f), compress_level=1)
        if check:
            ref = np.array(Image.open(os.path.join(emu_dir, "frames", "%05d.png" % f)))
            if not np.array_equal(ref, rgb):
                mism.append(f)
        if f % 500 == 0:
            print("frame", f, file=sys.stderr)
    for o in ops[oi:]:                             # after the last frame: music fade-out
        if o[1] == "music" and o[2] == "fade" and o[3] & 0x80000000:
            music_fade = nframes
    sfx_all = np.concatenate(sfx_stream[:nframes]).astype(np.int8)
    open(os.path.join(OUT, "sfx_ref.raw"), "wb").write(sfx_all.tobytes())
    if check:
        ref = np.frombuffer(ref_sfx[:sfx_all.size], np.int8)
        bad = int(np.count_nonzero(ref != sfx_all[:ref.size]))
        print("frames compared: %d, different: %d %s" % (nframes, len(mism), mism[:20]))
        print("effect samples compared: %d, different: %d" % (ref.size, bad))
    # ---- audio: music (ch 0 left, 1+2 right) + effects (ch 3 left)
    tail = MUSIC_FADE_FRAMES if music_fade is not None else 0
    total = nframes + tail
    L, Rr = music_track(total, music_start or 0, music_fade)
    # effects: 160 samples per frame -> resample onto the 50 Hz timeline (pitch +0.8 %)
    idx = np.minimum((np.arange(L.size) * 8000.0 / OUT_RATE).astype(np.int64), sfx_all.size - 1)
    fx = np.where(np.arange(L.size) < nframes * OUT_RATE // 50, sfx_all[idx].astype(np.float32), 0)
    L = L + fx
    st = np.empty(L.size * 2, np.float32)
    st[0::2] = L
    st[1::2] = Rr
    pcm = np.clip(st * (32767.0 / 128.0 / 2.0) * 1.6, -32768, 32767).astype("<i2")
    wav = os.path.join(OUT, "intro_ref.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(OUT_RATE)
        w.writeframes(pcm.tobytes())
    print("wrote", wav, "%.1f s" % (L.size / OUT_RATE))
    # black frames for the final music fade
    if png:
        black = np.zeros((256, 320, 3), np.uint8)
        for f in range(nframes, total):
            Image.fromarray(black).save(os.path.join(fdir, "%05d.png" % f))
    ff = shutil.which("ffmpeg")
    if ff and png:
        mp4 = os.path.join(OUT, "intro_ref.mp4")
        subprocess.run([ff, "-y", "-loglevel", "error", "-framerate", "50", "-i",
                        os.path.join(fdir, "%05d.png"), "-i", wav, "-vf",
                        "scale=640:512:flags=neighbor", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                        "-crf", "18", "-c:a", "aac", "-b:a", "192k", "-shortest", mp4], check=True)
        print("wrote", mp4)


if __name__ == "__main__":
    main()
