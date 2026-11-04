"""Print the scene list of the intro (from SD00) and each scene's sound-effect timetable.

Scene order: SD00 main entry ($328) calls the scene routines one after another.
Sound timetable (SD00 $3c4/$3d6): 8-byte entries, big endian words
    tick, sample (1-based index into the SMPx bank), volume (0..$3f), repeat count
repeat: number of plays, $ffff = loop (until the scene stops the mixer).
One entry is checked per scene tick (one tick = $92 vertical blanks, see TICK below);
the list has no terminator, it simply stops matching when the next tick value is never reached
(tick values must increase).
"""
from introlib import load, be16

SCENES = [
    # (routine, name, bank, files, sound table, vblanks per tick)
    (0x0F70, "1", "SMP0", "US00 (PIC0, GA00, SPR1, FHRS), MUSI start", 0x10D2, 6),
    (0x1A28, "2", "SMP0", "GA01, PIB1", 0x1BDC, 6),
    (0x5098, "3", "SMP2", "GAB2, US02 (PIC2, GA02); later GAC2 + SML2", 0x522E, 6),
    (0x5C6E, "4 (disk 2)", "SMP3", "US03 (PIC3, GA03), KPIC, KMSK", 0x5D8A, 7),
    (0x60AA, "5 (disk 2)", "SMP4", "US04 (PIC4, GA04)", 0x6208, 6),
    (0x35EE, "6 (disk 2)", "SMP5", "GA01, PIB5", 0x37B4, 6),
    (0x6704, "7 (disk 2)", None, "US06 (PIC6), 750 vblanks", None, 1),
]


def table(sd, addr, maxn=64):
    out = []
    last = -1
    for i in range(maxn):
        t, s, v, r = (be16(sd, addr + 8 * i + 2 * k) for k in range(4))
        if t <= last or t > 0x2000 or s == 0 or s > 16 or v > 0x40:
            break
        out.append((t, s, v, r))
        last = t
    return out


def main():
    sd = load("SD00")
    import samples
    names = {}
    for bank in set(s[2] for s in SCENES if s[2]):
        _, items = samples.bank(bank)
        names[bank] = {k: inf["name"] for k, _, _, inf in items}
    for rout, name, bank, files, tab, vb in SCENES:
        print("Scene %s: SD00 $%04x, tick = %d vblanks (%.2f Hz), samples %s, %s"
              % (name, rout, vb, 50.0 / vb, bank, files))
        if tab is None:
            continue
        for t, s, v, r in table(sd, tab):
            print("   tick %4d (%6.2f s)  sample %d %-12s vol %2d  %s"
                  % (t, t * vb / 50.0, s, names[bank].get(s, "?"), v,
                     "loop" if r == 0xFFFF else "x%d" % r))


if __name__ == "__main__":
    main()
