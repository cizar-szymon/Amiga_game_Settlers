"""Opis okna obsady budynków wojskowych: okno 144x160 od (248,160)."""
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
shot = Image.open(src).convert("RGB")
BX, BY = 248, 160
S = 5
box = shot.crop((BX, BY, BX + 144, BY + 160)).resize((144 * S, 160 * S), Image.NEAREST)

W, H = 1900, 1010
OX, OY = 560, 120
img = Image.new("RGB", (W, H), (245, 245, 240))
img.paste(box, (OX, OY))
d = ImageDraw.Draw(img)
f = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 18)
fs = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 15)
ft = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 24)
d.text((20, 15), "Okno obsady budynków wojskowych (Knight level) – Settlers 0.137", font=ft, fill=(20, 20, 20))
d.text((20, 50), "Wiersze A–D: budynki od najbliższych wrogowi (A) do leżących w głębi kraju (D). "
       "Górny napis = OBSADA, dolny = REZERWA.", font=fs, fill=(60, 60, 60))


def pt(x, y):
    return OX + (8 + x) * S, OY + (8 + y) * S


rows = [("A", "najbliżej wroga"), ("B", "blisko wroga"), ("C", "daleko od wroga"), ("D", "w głębi kraju")]
items = []
n = 1
for i, (letter, where) in enumerate(rows):
    y0 = 2 + 34 * i
    items += [
        (n, (16, y0 + 14), "L", f"{letter}: obrazek – {where}", "(bez funkcji)"),
        (n + 1, (40, y0 + 8), "L", f"{letter}: obsada −", "mniej rycerzy w budynku"),
        (n + 2, (56, y0 + 8), "R", f"{letter}: obsada +", "więcej rycerzy w budynku"),
        (n + 3, (100, y0 + 9), "R", f"{letter}: obsada (napis)", "ilu rycerzy budynek chce"),
        (n + 4, (40, y0 + 24), "L", f"{letter}: rezerwa −", "mniej zostaje przy ataku"),
        (n + 5, (56, y0 + 24), "R", f"{letter}: rezerwa +", "więcej zostaje przy ataku"),
        (n + 6, (100, y0 + 20), "R", f"{letter}: rezerwa (napis)", "ilu zostaje w obronie przy ataku"),
    ]
    n += 7
items.append((n, (120, 136), "R", "EXIT", "powrót do wyboru ustawień"))

left = [it for it in items if it[2] == "L"]
right = [it for it in items if it[2] == "R"]
RED = (200, 20, 20)


def draw(lst, side, top, step):
    for k, (nr, (x, y), _, name, desc) in enumerate(lst):
        ly = top + k * step
        px, py = pt(x, y)
        if side == "L":
            ax = OX - 14
            cx = ax - 20
        else:
            ax = OX + 144 * S + 14
            cx = ax + 20
        d.line([(ax, ly + 12), (px, py)], fill=RED, width=2)
        d.ellipse([px - 5, py - 5, px + 5, py + 5], outline=RED, width=3)
        d.ellipse([cx - 15, ly - 3, cx + 15, ly + 27], fill=RED)
        t = str(nr)
        d.text((cx - d.textlength(t, font=f) / 2, ly + 1), t, font=f, fill=(255, 255, 255))
        if side == "L":
            d.text((cx - 22 - d.textlength(name, font=f), ly - 4), name, font=f, fill=(10, 10, 10))
            d.text((cx - 22 - d.textlength(desc, font=fs), ly + 17), desc, font=fs, fill=(70, 70, 70))
        else:
            d.text((cx + 22, ly - 4), name, font=f, fill=(10, 10, 10))
            d.text((cx + 22, ly + 17), desc, font=fs, fill=(70, 70, 70))


draw(left, "L", 110, 72)
draw(right, "R", 95, 52)
img.save(out)
print(out, img.size)
