"""Print a range of a disassembly listing: python show.py FILE.s START END (hex), compact."""
import sys
fn, a, b = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
for line in open(fn):
    try:
        ad = int(line[:6], 16)
    except ValueError:
        continue
    if a <= ad < b:
        print(line[:8] + line[29:].split("    ;")[0].rstrip())
