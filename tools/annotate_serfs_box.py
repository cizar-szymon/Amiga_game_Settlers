"""Opis okna statystyki osadników (draw_stat_6_box): okno 144x160 od (248,160)
na zrzucie 640x480 (F9). python tools/annotate_serfs_box.py zrzut.png opis.png"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
shot = Image.open(src).convert("RGB")
BX, BY = 248, 160
S = 5
box = shot.crop((BX, BY, BX + 144, BY + 160)).resize((144 * S, 160 * S), Image.NEAREST)

W, H = 1950, 960
OX, OY = 380, 110
img = Image.new("RGB", (W, H), (245, 245, 240))
img.paste(box, (OX, OY))
d = ImageDraw.Draw(img)
f = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 18)
fs = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 15)
ft = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 24)
d.text((20, 15), "Okno statystyki osadników (Serfs) – Settlers 0.137", font=ft, fill=(20, 20, 20))
d.text((20, 50), "Obrazek = zawód, liczba obok = ilu osadników tego zawodu ma gracz (w budynkach, w drodze i w magazynach).",
       font=fs, fill=(60, 60, 60))


def pt(x, y):
    return OX + (8 + x) * S, OY + (8 + y) * S


col1 = ["Tragarz", "Przewoźnik (łódź)", "Kopacz", "Budowniczy", "Rycerz poz. 4",
        "Rycerz poz. 3", "Rycerz poz. 2", "Rycerz poz. 1", "Rycerz poz. 0"]
col2 = ["Drwal", "Tracz", "Hutnik", "Kamieniarz", "Leśniczy", "Górnik", "Szkutnik",
        "Narzędziowiec", "Płatnerz"]
col3 = ["Rybak", "Hodowca świń", "Rzeźnik", "Rolnik", "Młynarz", "Piekarz", "Geolog",
        "Wolny osadnik"]
items = []
n = 1
for c, names in ((0, col1), (1, col2), (2, col3)):
    for r, name in enumerate(names):
        items.append((n, (8 + 40 * c + 8, 16 * r + 8), name))
        n += 1
items.append((n, (100, 136), "Razem (wszyscy osadnicy)")); n += 1
items.append((n, (120, 136), "EXIT"))

RED = (200, 20, 20)
# numery na obrazkach (lewy górny róg obrazka zawodu)
for nr, (x, y), name in items:
    px, py = pt(x - 10, y)
    if name in ("EXIT",):
        px, py = pt(x - 2, y - 6)
    if name.startswith("Razem"):
        px, py = pt(x - 14, y - 6)
    d.ellipse([px - 15, py - 15, px + 15, py + 15], fill=RED, outline=(255, 255, 255), width=2)
    t = str(nr)
    d.text((px - d.textlength(t, font=f) / 2, py - 11), t, font=f, fill=(255, 255, 255))

# legenda
LX = OX + 144 * S + 50
for k, (nr, _, name) in enumerate(items):
    col, row = divmod(k, 14)
    x = LX + col * 300
    y = 130 + row * 56
    d.ellipse([x, y, x + 30, y + 30], fill=RED)
    t = str(nr)
    d.text((x + 15 - d.textlength(t, font=f) / 2, y + 4), t, font=f, fill=(255, 255, 255))
    d.text((x + 40, y + 5), name, font=f, fill=(10, 10, 10))
d.text((20, 130), "Kolumny okna:", font=f, fill=(10, 10, 10))
d.text((20, 160), "lewa: tragarze, budowa, rycerze", font=fs, fill=(60, 60, 60))
d.text((20, 185), "środkowa: drewno, kamień, metal,", font=fs, fill=(60, 60, 60))
d.text((20, 205), "    narzędzia, broń", font=fs, fill=(60, 60, 60))
d.text((20, 230), "prawa: żywność, geolog, wolni", font=fs, fill=(60, 60, 60))
d.text((20, 270), "Liczba = osadnicy tego zawodu", font=fs, fill=(60, 60, 60))
d.text((20, 290), "(pracujący, w drodze, w magazynach).", font=fs, fill=(60, 60, 60))
img.save(out)
print(out, img.size)
