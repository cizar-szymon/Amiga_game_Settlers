"""Prints the tempo commands (Fxx) of a ProTracker module in song order.

usage: python tools/modinfo.py FILE.mod
Fxx below 32 sets the speed (ticks per row), 32 and above the BPM (CIA
timing); a vertical-blank player treats every Fxx as a speed and keeps
125 BPM.
"""
import sys


def main():
    d = open(sys.argv[1], "rb").read()
    name = d[0:20].split(b"\0")[0].decode("latin-1")
    song_len = d[950]
    order = list(d[952:952 + 128])
    tag = d[1080:1084].decode("latin-1", "replace")
    patterns = max(order) + 1
    print("name %r, tag %s, song length %d, patterns %d" %
          (name, tag, song_len, patterns))
    print("order:", order[:song_len])
    seen = {}
    for pos in range(song_len):
        p = order[pos]
        base = 1084 + p * 1024
        for row in range(64):
            for ch in range(4):
                o = base + row * 16 + ch * 4
                cmd = d[o + 2] & 0x0F
                par = d[o + 3]
                if cmd == 0xF:
                    kind = "speed" if par < 32 else "BPM"
                    key = (kind, par)
                    seen.setdefault(key, []).append((pos, p, row, ch))
    for (kind, par), where in sorted(seen.items()):
        print("F%02X (%s %d): %d times, first at song pos %d pattern %d row %d"
              % (par, kind, par, len(where), where[0][0], where[0][1],
                 where[0][2]))


if __name__ == "__main__":
    main()
