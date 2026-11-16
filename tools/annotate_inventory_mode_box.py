"""Opis okna trybów magazynu / zamku (draw_resdir_box): okno 144x160 od (248,160)
na zrzucie 640x480 (F9). python tools/annotate_inventory_mode_box.py zrzut.png opis.png"""
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
d.text((20, 15), "Okno trybów magazynu / zamku – Settlers 0.137", font=ft, fill=(20, 20, 20))
d.text((20, 50), "Zaznaczenie = wybrany tryb. IN: lewy klik; STOP i OUT: PRAWY klik (jak w oryginale). "
       "Liczba przy rycerzu = obrońcy zamku tego poziomu.", font=fs, fill=(60, 60, 60))


def pt(x, y):
    return OX + (8 + x) * S, OY + (8 + y) * S


items = [
    (1, (30, 30), "Obrazek: towary"),
    (2, (64, 24), "Towary: przyjmuj (IN)"),
    (3, (64, 40), "Towary: wstrzymaj (STOP)"),
    (4, (64, 56), "Towary: wywoź (OUT)"),
    (5, (30, 94), "Obrazek: osadnicy"),
    (6, (64, 88), "Osadnicy: przyjmuj (IN)"),
    (7, (64, 104), "Osadnicy: wstrzymaj (STOP)"),
    (8, (64, 120), "Osadnicy: wysyłaj (OUT)"),
    (9, (92, 24), "Obrońcy zamku: rycerz poz. 4"),
    (10, (92, 44), "Obrońcy zamku: rycerz poz. 3"),
    (11, (92, 64), "Obrońcy zamku: rycerz poz. 2"),
    (12, (92, 84), "Obrońcy zamku: rycerz poz. 1"),
    (13, (92, 104), "Obrońcy zamku: rycerz poz. 0"),
    (14, (92, 136), "Przewiń: okno towarów zamku"),
    (15, (110, 128), "EXIT"),
]
NOTES = ["Tryby:", "IN – magazyn przyjmuje", "STOP – nie przyjmuje,", "    trzyma, co ma",
         "OUT – nie przyjmuje", "    i wywozi do innych", "", "Rycerze 9–13 tylko", "w oknie zamku."]

RED = (200, 20, 20)
for nr, (x, y), name in items:
    px, py = pt(x, y)
    d.ellipse([px - 15, py - 15, px + 15, py + 15], fill=RED, outline=(255, 255, 255), width=2)
    t = str(nr)
    d.text((px - d.textlength(t, font=f) / 2, py - 11), t, font=f, fill=(255, 255, 255))

LX = OX + 144 * S + 60
for k, (nr, _, name) in enumerate(items):
    x, y = LX, 130 + k * 50
    d.ellipse([x, y, x + 30, y + 30], fill=RED)
    t = str(nr)
    d.text((x + 15 - d.textlength(t, font=f) / 2, y + 4), t, font=f, fill=(255, 255, 255))
    d.text((x + 40, y + 5), name, font=f, fill=(10, 10, 10))
for k, line in enumerate(NOTES):
    d.text((20, 130 + 24 * k), line, font=f if k == 0 else fs,
           fill=(10, 10, 10) if k == 0 else (60, 60, 60))
img.save(out)
print(out, img.size)
