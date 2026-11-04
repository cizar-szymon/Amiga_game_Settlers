"""Summarise the choreography of every intro scene from the emulator log (build/intro/emu/).

For each scene: start/end frame and length, fades (EFEC), sound effects (frame, sample name,
volume, repeats), bobs drawn through DISP -$54 grouped by source (file + entry, size) with the
frame range and the x/y range, frame-difference animations, and scrolling (shown bitmap row).
Writes build/intro/choreography.txt and build/intro/choreography.json.
"""
import json
import os
from introlib import OUT, be32, load
import samples

SCENE_NAMES = {1: "las (PIC0, przewijanie)", 2: "dziurka od klucza (PIB1)", 3: "gospodarstwo (PIC2)",
               4: "kuznia (PIC3)", 5: "zamek i most (PIC4)", 6: "brama (PIB5)", 7: "napis (PIC6)"}


def main():
    ev = json.load(open(os.path.join(OUT, "emu", "log.json")))
    views = json.load(open(os.path.join(OUT, "emu", "views.json")))
    loads = []                                  # (addr, size, name)
    scenes = []
    cur = None
    bank_names = {}
    for e in ev:
        k = e["k"]
        if k == "load":
            loads.append((e["addr"], e["size"], e["name"]))
        if k.startswith("scene"):
            cur = {"scene": int(k[5:]), "start": e["f"], "fades": [], "sfx": [], "bobs": {},
                   "deltas": {}, "files": []}
            scenes.append(cur)
        if cur is None:
            continue
        if k == "load":
            cur["files"].append(e["name"])
        elif k.startswith("EFEC_"):
            cur["fades"].append([e["f"], {"EFEC_24": "in", "EFEC_2c": "out"}.get(k, k), e["d0"]])
        elif k == "sfx":
            cur["sfx"].append([e["f"], e["d0"], e["d1"], e["d2"] & 0xFFFF])
        elif k == "DISP_blit":
            src = e["d0"]
            name, entry = where(loads, src)
            key = "%s#%s %dx%d%s" % (name, entry, e["d3"], e["d4"], " mask" if e["a1"] else "")
            x = e["d1"] & 0xFFFF
            x = x - 0x10000 if x & 0x8000 else x
            y = e["d2"] & 0xFFFF
            y = y - 0x10000 if y & 0x8000 else y
            b = cur["bobs"].setdefault(key, [e["f"], e["f"], x, x, y, y, 0])
            b[1] = e["f"]
            b[2], b[3] = min(b[2], x), max(b[3], x)
            b[4], b[5] = min(b[4], y), max(b[5], y)
            b[6] += 1
        elif k in ("delta3", "delta4"):
            name, entry = where(loads, e["a0"])
            d = cur["deltas"].setdefault("%s#%s" % (name, entry), [e["f"], e["f"], 0])
            d[1] = e["f"]
            d[2] += 1
        elif k == "end":
            pass
    for i, s in enumerate(scenes):
        s["end"] = scenes[i + 1]["start"] if i + 1 < len(scenes) else len(views)
        s["frames"] = s["end"] - s["start"]
        bank = next((f for f in s["files"] if f.startswith("SM")), None)
        if bank:
            _, items = samples.bank(bank)
            names = {kk: inf["name"] for kk, _, _, inf in items}
            for x in s["sfx"]:
                x.append(names.get(x[1], "?"))
            s["bank"] = bank
    lines = []
    for s in scenes:
        n = s["scene"]
        lines.append("== Scena %d: %s - klatki %d..%d (%d = %.1f s), pliki %s" % (
            n, SCENE_NAMES.get(n, ""), s["start"], s["end"], s["frames"], s["frames"] / 50.0,
            " ".join(s["files"])))
        for f, kind, steps in s["fades"]:
            lines.append("   fade %-3s  klatka %5d (+%d)  kroki %d" % (kind, f, f - s["start"], steps))
        for f, smp, vol, rep, *nm in s["sfx"]:
            lines.append("   sfx       klatka %5d (+%d)  %s#%d %s vol %d %s" % (
                f, f - s["start"], s.get("bank"), smp, nm[0] if nm else "", vol,
                "petla" if rep == 0xFFFF else "x%d" % rep))
        for key, (f0, f1, x0, x1, y0, y1, cnt) in sorted(s["bobs"].items(), key=lambda kv: kv[1][0]):
            lines.append("   bob %-28s klatki %5d..%5d  x %4d..%4d  y %4d..%4d  %d razy" % (
                key, f0 - s["start"], f1 - s["start"], x0, x1, y0, y1, cnt))
        for key, (f0, f1, cnt) in s["deltas"].items():
            lines.append("   roznice %-14s klatki %5d..%5d  %d razy" % (key, f0 - s["start"],
                                                                     f1 - s["start"], cnt))
        # scrolling: plane-0 pointer of the first band changes inside one block
        offs = []
        for f in range(s["start"], s["end"]):
            if views[f]:
                p = views[f][0][1][0]
                offs.append((f, tuple(p), views[f][0][3], len(views[f])))
        moves = sum(1 for a, b in zip(offs, offs[1:]) if a[1][0] == b[1][0] and a[1][1] != b[1][1])
        bands = max((o[3] for o in offs), default=0)
        lines.append("   widok: %d zmian przesuniecia obrazu w obrebie bitmapy, maks. %d pasy na klatke"
                     % (moves, bands))
    txt = "\n".join(lines)
    print(txt)
    open(os.path.join(OUT, "choreography.txt"), "w", encoding="utf-8").write(txt + "\n")
    json.dump(scenes, open(os.path.join(OUT, "choreography.json"), "w"), indent=1)


def where(loads, addr):
    for a, size, name in reversed(loads):
        if a <= addr < a + size:
            off = addr - a
            if name.startswith("GA") or name.startswith("KP") or name.startswith("KM"):
                b = load(name)
                if name.startswith("GA"):
                    offs = [be32(b, 4 + 4 * i) for i in range((be32(b, 4) - 4) // 4)]
                    best = None
                    for i, o in enumerate(offs):
                        if 0 < o <= off and (best is None or o >= offs[best]):
                            best = i
                    if best is not None:
                        d = off - offs[best]
                        return name, str(best) if d == 0 else "%d+%d" % (best, d)
            return name, "+%d" % off
    return "bufor", "%x" % addr


if __name__ == "__main__":
    main()
