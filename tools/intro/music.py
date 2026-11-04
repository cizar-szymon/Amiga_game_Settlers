"""The Settlers intro music (MUSI): parse, export instruments, render the song.

MUSI (after the disk RLE) is a plain 31-instrument ProTracker module ("M.K."), title "nintro5".
The SOUN module contains the standard ProTracker 2.x replay routine (mt_init/mt_music at SOUN
file offsets $6e6/$786, period table at $11e4), changed in three ways (verified in SOUN.s):
  * only Paula channels 0..2 are played ($a0/$b0/$c0); the 4th pattern column is skipped
    (it is empty in MUSI) - channel 3 belongs to SD00's own 3-voice sample mixer;
  * every volume goes through a 65-byte master volume table (SOUN $17a2) used for fades;
  * the song position wraps to 0 at the end (the restart byte is ignored); Fxx sets ticks only.
Timing: mt_music runs from a VERTB interrupt server (SOUN $566: intnum 5) -> 50 Hz PAL.

usage: python music.py            -> build/intro/music/*.wav, prints the song structure
       python music.py --no-render  (skip the song render)
"""
import os
import struct
import sys
import wave

from introlib import load, outdir

PAULA_PAL = 3546895          # Paula clock (PAL), Hz
C3_RATE = PAULA_PAL // 214   # 16574 Hz: ProTracker C-3 (period 214); instruments are tuned to C-3
TICK_HZ = 50                 # VBlank
OUT_RATE = 22050

NOTE_NAMES = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"]
# ProTracker finetune 0 periods C-1..B-3 (same as the first 36 words of SOUN $11e4)
PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]


def be16(b, o):
    return (b[o] << 8) | b[o + 1]


def parse(d):
    mod = {"title": d[0:20].split(b"\0")[0].decode("latin-1"), "samples": []}
    for i in range(31):
        o = 20 + 30 * i
        mod["samples"].append({
            "name": d[o:o + 22].split(b"\0")[0].decode("latin-1"),
            "len": be16(d, o + 22) * 2, "finetune": d[o + 24] & 15, "vol": d[o + 25],
            "rep": be16(d, o + 26) * 2, "replen": be16(d, o + 28) * 2})
    mod["songlen"] = d[950]
    mod["restart"] = d[951]
    mod["order"] = list(d[952:952 + 128])
    assert d[1080:1084] == b"M.K.", "not a ProTracker module"
    npat = max(mod["order"]) + 1
    mod["patterns"] = []
    for p in range(npat):
        rows = []
        for r in range(64):
            row = []
            for ch in range(4):
                o = 1084 + p * 1024 + r * 16 + ch * 4
                b = d[o:o + 4]
                row.append(((b[0] & 0xF0) | (b[2] >> 4), ((b[0] & 15) << 8) | b[1], b[2] & 15, b[3]))
            rows.append(row)
        mod["patterns"].append(rows)
    p = 1084 + npat * 1024
    for s in mod["samples"]:
        s["data"] = bytearray(d[p:p + s["len"]])
        p += s["len"]
    mod["end"] = p
    assert p == len(d), "size mismatch %d != %d" % (p, len(d))
    # mt_init clears the first longword of every sample (SOUN $726: clr.l (a2))
    for s in mod["samples"]:
        s["data"][0:4] = bytes(min(4, len(s["data"])))
    return mod


def note_name(per):
    if per == 0:
        return "---"
    if per in PERIODS:
        i = PERIODS.index(per)
        return "%s%d" % (NOTE_NAMES[i % 12], i // 12 + 1)
    return "%03x" % per


def write_wav(path, data, rate, channels=1, width=1):
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(width)
        w.setframerate(rate)
        w.writeframes(data)


def export_samples(mod, d):
    for i, s in enumerate(mod["samples"]):
        if not s["len"]:
            continue
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in s["name"]) or "noname"
        # WAV 8-bit is unsigned: flip the sign bit of the Amiga signed bytes
        write_wav(os.path.join(d, "%02d_%s.wav" % (i + 1, safe)),
                  bytes((b + 128) & 255 for b in s["data"]), C3_RATE)


def print_structure(mod):
    print("title %r, song length %d, restart byte %d (ignored by SOUN), %d patterns"
          % (mod["title"], mod["songlen"], mod["restart"], len(mod["patterns"])))
    print("order:", mod["order"][:mod["songlen"]])
    print("instruments (len/rep/replen in bytes):")
    for i, s in enumerate(mod["samples"]):
        if s["len"] or s["name"]:
            print("  %2d %-22s len %6d vol %2d fine %d rep %5d replen %5d"
                  % (i + 1, s["name"], s["len"], s["vol"], s["finetune"], s["rep"], s["replen"]))
    rows = mod["songlen"] * 64
    print("speed 6 (no Fxx in the song) -> %d rows x 6 ticks / 50 Hz = %.1f s per loop"
          % (rows, rows * 6 / TICK_HZ))
    print("pattern 0, first 16 rows (sample note effect+param, columns 1-3; column 4 empty):")
    for r in range(16):
        print("  %02d  " % r + "  |  ".join(
            "%02d %s %X%02X" % (smp, note_name(per), e, a) for smp, per, e, a in mod["patterns"][0][r][:3]))


class Voice:
    def __init__(self):
        self.smp = None
        self.data = b""
        self.pos = 0.0
        self.end = 0       # end of the current play range (bytes)
        self.loop_start = 0
        self.loop_len = 0  # bytes; <= 2 means "one-shot" (loop on the cleared first word)
        self.period = 0
        self.vol = 0
        self.on = False


def render(mod, seconds_cap=None, loops=1):
    """Render the song (3 voices, ProTracker 2 note/volume semantics, only effect C used)."""
    import numpy as np
    voices = [Voice() for _ in range(3)]
    samp_per_tick = OUT_RATE / TICK_HZ
    out_l, out_r = [], []
    speed = 6
    total_rows = mod["songlen"] * 64 * loops
    acc = 0.0
    for rowi in range(total_rows):
        pos = (rowi // 64) % mod["songlen"]
        row = mod["patterns"][mod["order"][pos]][rowi % 64]
        for ch in range(3):
            smp, per, eff, arg = row[ch]
            v = voices[ch]
            if smp:
                s = mod["samples"][smp - 1]
                v.smp = s
                v.vol = s["vol"]
            if per and v.smp is not None:
                s = v.smp
                v.data = s["data"]
                if s["rep"]:
                    v.end = min(s["rep"] + s["replen"], len(s["data"]))
                else:
                    v.end = len(s["data"])
                v.loop_start = s["rep"]
                v.loop_len = s["replen"]
                v.pos = 0.0
                v.period = per
                v.on = len(v.data) > 0
            if eff == 0xC:
                v.vol = min(arg, 64)
            elif eff == 0xF and arg:
                speed = arg
        for t in range(speed):
            acc += samp_per_tick
            n = int(acc)
            acc -= n
            bufs = []
            for v in voices:
                buf = np.zeros(n, dtype=np.float32)
                if v.on and v.period and v.vol:
                    step = PAULA_PAL / v.period / OUT_RATE
                    arr = np.frombuffer(bytes(v.data), dtype=np.int8).astype(np.float32)
                    k = 0
                    while k < n:
                        if v.pos >= v.end:
                            if v.loop_len > 2:
                                v.pos = v.loop_start + (v.pos - v.end) % v.loop_len
                                v.end = v.loop_start + v.loop_len
                            else:
                                v.on = False
                                break
                        cnt = min(n - k, int((v.end - v.pos) / step) + 1)
                        idx = (v.pos + step * np.arange(cnt)).astype(np.int64)
                        idx = np.minimum(idx, v.end - 1)
                        buf[k:k + cnt] = arr[idx] * (v.vol / 64.0)
                        v.pos += step * cnt
                        k += cnt
                bufs.append(buf)
            # Amiga stereo: channels 0 and 3 left, 1 and 2 right
            out_l.append(bufs[0])
            out_r.append(bufs[1] + bufs[2])
        if seconds_cap and len(out_l) * samp_per_tick / OUT_RATE >= seconds_cap:
            break
    left = np.concatenate(out_l)
    right = np.concatenate(out_r)
    st = np.empty(left.size * 2, dtype=np.float32)
    st[0::2] = left
    st[1::2] = right
    pcm = np.clip(st * (32767.0 / 128.0 / 2.0) * 1.6, -32768, 32767).astype("<i2")
    return pcm, left, right


def plot_waveform(path, left, right):
    from PIL import Image, ImageDraw
    import numpy as np
    w, h = 1200, 300
    img = Image.new("RGB", (w, h), (255, 255, 255))
    dr = ImageDraw.Draw(img)
    for idx, (sig, y0, col) in enumerate(((left, 75, (0, 0, 160)), (right, 225, (160, 0, 0)))):
        n = sig.size // w
        for x in range(w):
            seg = sig[x * n:(x + 1) * n]
            if seg.size:
                lo, hi = float(seg.min()), float(seg.max())
                dr.line([(x, y0 - hi / 256 * 70), (x, y0 - lo / 256 * 70)], fill=col)
    img.save(path)


def main():
    d = load("MUSI")
    mod = parse(d)
    od = outdir("music")
    print_structure(mod)
    export_samples(mod, od)
    if "--no-render" not in sys.argv:
        pcm, left, right = render(mod)
        write_wav(os.path.join(od, "intro_song.wav"), pcm.tobytes(), OUT_RATE, channels=2, width=2)
        plot_waveform(os.path.join(od, "intro_song.png"), left, right)
        print("rendered %.1f s -> %s" % (left.size / OUT_RATE, os.path.join(od, "intro_song.wav")))


if __name__ == "__main__":
    main()
