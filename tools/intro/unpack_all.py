"""Unpack every RLE file of the intro disk to build/intro/raw/<NAME>.bin and print sizes."""
import os
from introlib import DATA, load, outdir

NAMES = ["DISP", "EFEC", "FUNC", "INIT", "INPT", "SOUN", "USER", "FHRS", "SD00", "SCPT",
         "PIC0", "PIC2", "PIB1", "GA00", "GA01", "GA02", "GAB2", "GAC2", "SML2", "SMP0",
         "SMP2", "MUSI", "SPR1", "US00", "US02",
         # disk 2
         "GA03", "GA04", "GA06", "KMSK", "KPIC", "PIB5", "PIC3", "PIC4", "PIC6", "SMP3", "SMP4",
         "SMP5", "US03", "US04", "US05", "US06"]


def main():
    d = outdir("raw")
    for n in NAMES:
        if n.startswith("PIC"):
            # PICx are ByteRun1 (PackBits) on disk, not RLE: decoded by gfx.py / gfx_ga.py
            continue
        raw = load(n, packed=False)
        un = load(n)
        with open(os.path.join(d, n + ".bin"), "wb") as f:
            f.write(un)
        print("%-5s esc=%02x packed=%6d unpacked=%6d" % (n, raw[0], len(raw), len(un)))


if __name__ == "__main__":
    main()
