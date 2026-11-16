"""Opis okna ustawień rycerzy: zrzut 640x480, okno 144x160 od (248,160)."""
import sys
from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
shot = Image.open(src).convert("RGB")
BX, BY = 248, 160          # lewy górny róg okna na ekranie
CX, CY = BX + 8, BY + 8    # początek wnętrza (współrzędne przycisków gry)
S = 4                      # powiększenie
box = shot.crop((BX, BY, BX + 144, BY + 160)).resize((144 * S, 160 * S), Image.NEAREST)

W, H = 1500, 760
OX, OY = 462, 60           # miejsce okna na obrazie
img = Image.new("RGB", (W, H), (245, 245, 240))
img.paste(box, (OX, OY))
d = ImageDraw.Draw(img)
try:
    f = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 19)
    fs = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 16)
    ft = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 24)
except OSError:
    f = fs = ft = ImageFont.load_default()

d.text((20, 15), "Okno ustawień rycerzy (Knight settings) – Settlers 0.137", font=ft, fill=(20, 20, 20))


def pt(x, y):
    """Punkt wnętrza okna (piksele gry) -> obraz."""
    return OX + (CX - BX + x) * S, OY + (CY - BY + y) * S


# nr, punkt w oknie (x, y wnętrza), strona, y etykiety, nazwa, opis
items = [
    (1, (24, 16), "L", 95, "Osadnik", "(obrazek – koniec suwaka: mniej rycerzy)"),
    (2, (64, 16), "R", 95, "Suwak: osadnicy -> rycerze", "ilu nowych osadników ma zostać rycerzami"),
    (3, (104, 16), "R", 150, "Rycerz", "(obrazek – koniec suwaka: więcej rycerzy)"),
    (4, (24, 36), "L", 165, "Szkól 1", "zrób 1 rycerza teraz"),
    (5, (40, 36), "L", 215, "Szkól 5", "zrób 5 rycerzy teraz"),
    (6, (24, 52), "L", 265, "Szkól 20", "zrób 20 rycerzy teraz"),
    (7, (40, 52), "L", 315, "Szkól 100", "zrób 100 rycerzy teraz"),
    (8, (72, 36), "R", 205, "Osadnik + tarcza + miecz", "(obrazek: z czego powstaje rycerz)"),
    (9, (100, 44), "R", 260, "Gotowi do zamiany", "ilu osadników można zamienić teraz"),
    (10, (60, 66), "L", 375, "Morale rycerzy %", "siła rycerzy na cudzej ziemi"),
    (11, (52, 77), "R", 320, "Złoto", "sztabki w magazynach i strażnicach"),
    (12, (32, 72), "L", 430, "Słaby / silny rycerz", "(obrazki przy morale, bez funkcji)"),
    (13, (56, 92), "R", 380, "Atak: najsłabsi", "do ataku idą najpierw najsłabsi (teraz zaznaczone)"),
    (14, (56, 108), "R", 440, "Atak: najsilniejsi", "do ataku idą najpierw najsilniejsi"),
    (15, (96, 100), "R", 500, "Wymiana rycerzy", "słabszych z budynków zastąpią lepsi"),
    (16, (32, 128), "L", 490, "Mniej obrońców zamku", "-1 (najmniej 1)"),
    (17, (52, 123), "L", 550, "Obrońcy: chciani", "ilu rycerzy ma stać w zamku"),
    (18, (52, 133), "L", 610, "Obrońcy: są", "ilu jest teraz w zamku"),
    (19, (80, 128), "R", 560, "Więcej obrońców zamku", "+1 (najwięcej 99)"),
    (20, (120, 136), "R", 620, "EXIT", "powrót do wyboru ustawień"),
]

RED = (200, 20, 20)
for nr, (x, y), side, ly, name, desc in items:
    px, py = pt(x, y)
    if side == "L":
        tx = 20
        ax = OX - 14
    else:
        tx = OX + 144 * S + 40
        ax = OX + 144 * S + 14
    # strzałka
    d.line([(ax, ly + 12), (px, py)], fill=RED, width=3)
    d.ellipse([px - 6, py - 6, px + 6, py + 6], outline=RED, width=3)
    # numer w kółku
    cx = ax + (-20 if side == "L" else 20)
    d.ellipse([cx - 15, ly - 3, cx + 15, ly + 27], fill=RED)
    t = str(nr)
    tw = d.textlength(t, font=f)
    d.text((cx - tw / 2, ly + 1), t, font=f, fill=(255, 255, 255))
    if side == "L":
        d.text((tx, ly - 4), name, font=f, fill=(10, 10, 10))
        d.text((tx, ly + 18), desc, font=fs, fill=(70, 70, 70))
    else:
        d.text((cx + 22, ly - 4), name, font=f, fill=(10, 10, 10))
        d.text((cx + 22, ly + 18), desc, font=fs, fill=(70, 70, 70))

img.save(out)
print(out, img.size)
