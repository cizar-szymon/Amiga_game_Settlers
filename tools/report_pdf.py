"""Raport PDF: port Settlers (Freeserf) na Amigę RTG - zmiany i pomiary.

usage: python tools/report_pdf.py docs/Settlers-raport.pdf
Liczby z docs/PORTING.md (pomiary na prawdziwych Amigach przez AmiAgenta).
Styl jak raporty AmiXcom (UFO/raporty/report_short_pdf.py).
"""
import os
import sys
import tempfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from reportlab.lib import colors  # noqa: E402
from reportlab.lib.pagesizes import A4  # noqa: E402
from reportlab.lib.styles import ParagraphStyle  # noqa: E402
from reportlab.lib.units import mm  # noqa: E402
from reportlab.pdfbase import pdfmetrics  # noqa: E402
from reportlab.pdfbase.ttfonts import TTFont  # noqa: E402
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph,  # noqa: E402
                                SimpleDocTemplate, Spacer, Table, TableStyle)

OUT = sys.argv[1]
HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "..", "docs", "shots")

pdfmetrics.registerFont(TTFont("Arial", r"C:\Windows\Fonts\arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", r"C:\Windows\Fonts\arialbd.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Italic", r"C:\Windows\Fonts\ariali.ttf"))

BLUE = colors.HexColor("#1f5a9e")
GRID = colors.HexColor("#b0b7c0")
HEAD = colors.HexColor("#dfe7f2")

ss = {
    "title": ParagraphStyle("t", fontName="Arial-Bold", fontSize=18, leading=22, spaceAfter=2),
    "sub": ParagraphStyle("st", fontName="Arial", fontSize=9.5, leading=13,
                          textColor=colors.HexColor("#444444"), spaceAfter=8),
    "h2": ParagraphStyle("h2", fontName="Arial-Bold", fontSize=12.5, leading=16, spaceBefore=9,
                         spaceAfter=4, textColor=BLUE),
    "h3": ParagraphStyle("h3", fontName="Arial-Bold", fontSize=10, leading=13, spaceBefore=6,
                         spaceAfter=2),
    "p": ParagraphStyle("p", fontName="Arial", fontSize=9, leading=12.2, spaceAfter=3),
    "li": ParagraphStyle("li", fontName="Arial", fontSize=9, leading=12, leftIndent=10,
                         bulletIndent=0, spaceAfter=1.5),
    "small": ParagraphStyle("s", fontName="Arial", fontSize=7.5, leading=10,
                            textColor=colors.HexColor("#444444")),
    "cell": ParagraphStyle("c", fontName="Arial", fontSize=8.3, leading=10.3),
    "cellb": ParagraphStyle("cb", fontName="Arial-Bold", fontSize=8.3, leading=10.3),
    "gain": ParagraphStyle("g", fontName="Arial-Bold", fontSize=8.3, leading=10.3, alignment=2,
                           textColor=BLUE),
    "box": ParagraphStyle("box", fontName="Arial", fontSize=9.5, leading=13),
}


def P(t, s="p"):
    return Paragraph(t, ss[s])


def LI(t):
    return Paragraph(t, ss["li"], bulletText="•")


def num(v):
    return ("%g" % v).replace(".", ",")


def table(header, rows, widths, gain_col=None):
    data = [[P(h, "cellb") for h in header]]
    for r in rows:
        data.append([P(str(c), "gain" if i == gain_col else "cell") for i, c in enumerate(r)])
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1)
    t.setStyle(TableStyle([
        ("GRID", (0, 0), (-1, -1), 0.4, GRID),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("BACKGROUND", (0, 0), (-1, 0), HEAD),
        ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
    ]))
    return t


def boxed(paragraphs):
    t = Table([[paragraphs]], colWidths=[180 * mm])
    t.setStyle(TableStyle([
        ("BOX", (0, 0), (-1, -1), 0.8, BLUE),
        ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#f2f6fb")),
        ("LEFTPADDING", (0, 0), (-1, -1), 7), ("RIGHTPADDING", (0, 0), (-1, -1), 7),
        ("TOPPADDING", (0, 0), (-1, -1), 5), ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


# Wersja -> fps w scenie gry (Amiga 2, 640x480, bez sond), z PORTING.md
FPS = [
    ("0.09", 10.1, "start F4"),
    ("0.10", 11.4, "widok mapy w miejscu"),
    ("0.11", 17.9, "sprite'y przebiegami"),
    ("0.13", 22.7, "MOVE16, kafelki bez testów"),
    ("0.14", 24.3, "move.l zamiast memcpy"),
    ("0.16", 27.5, "klatka w VRAM"),
    ("0.17", 32.7, "sprite'y do VRAM słowami"),
    ("0.19", 36.3, "blokada na klatkę"),
    ("0.20", 46.3, "sprite jedną funkcją"),
    ("0.22", 42.4, "z dźwiękiem"),
]


def fps_chart(path):
    fig, ax = plt.subplots(figsize=(7.2, 3.0), dpi=150)
    names = [v for v, _, _ in FPS]
    vals = [f for _, f, _ in FPS]
    cols = ["#9aa9bd"] * len(FPS)
    cols[-2] = "#1f5a9e"
    cols[-1] = "#5c8fc9"
    bars = ax.bar(names, vals, color=cols)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 0.8, num(v), ha="center", fontsize=7.5)
    ax.axhline(50, color="#c0392b", lw=0.8, ls="--")
    ax.text(len(FPS) - 0.5, 50.8, "pułap 50 kl./s (krok logiki)", ha="right", fontsize=7,
            color="#c0392b")
    ax.set_ylim(0, 56)
    ax.set_ylabel("klatek na sekundę", fontsize=8)
    ax.set_xlabel("wersja portu", fontsize=8)
    ax.tick_params(labelsize=7.5)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.set_title("Scena gry 640×480, Amiga 2 (Warp7060, 68060 @ 100 MHz)", fontsize=8.5)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def main():
    tmp = tempfile.mkdtemp()
    chart = os.path.join(tmp, "fps.png")
    fps_chart(chart)

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15 * mm, rightMargin=15 * mm,
                            topMargin=14 * mm, bottomMargin=14 * mm,
                            title="Settlers na Amigę RTG – raport", author="Settlers port")
    st = []
    st.append(P("Settlers (Freeserf) na Amigę RTG", "title"))
    st.append(P("Raport zmian i pomiarów wydajności · wersja 0.32 beta · 25.10.2026", "sub"))

    st.append(boxed([
        P("<b>W skrócie.</b> The Settlers (Blue Byte 1993) działa na Amidze z kartą Picasso96 "
          "i 68040/68060 na bazie Freeserf, z oryginalnymi danymi z dyskietek na Amigę: "
          "obraz 8-bit do 800×600, muzyka i efekty przez Paulę, start z ikony.", "box"),
        Spacer(1, 3),
        P("Scena gry 640×480: <b>9,4 → 46,3 klatek/s</b> (bez dźwięku; z muzyką ~43), "
          "640×400: 49,9 (pułap 50), 800×600: 32,5. Logika z AI: ~0,6 ms na krok 20 ms "
          "(68060 @ 50 MHz). Każda szybka ścieżka sprawdzona trybem <i>VERIFY</i> – 0 różnic.",
          "box"),
    ]))

    st.append(P("Sprzęt i metoda", "h2"))
    st.append(LI("<b>Amiga 1</b> – A1200, Warp1260, 68060 @ 50 MHz: fazy F1–F3 (logika, dane, "
                 "pierwszy obraz). <b>Amiga 2</b> – Warp7060, 68060 @ 100 MHz (WARP Diagnostic): "
                 "optymalizacja F4, dźwięk F5, wydanie F6. Na A1200 liczby klatek będą niższe."))
    st.append(LI("Pomiary zawsze na prawdziwym sprzęcie przez AmiAgenta, w tej samej scenie: zapis "
                 "gry z misji 0 z zamkiem gracza (<i>simtest -c</i>), gra biegnie od razu po "
                 "wczytaniu."))
    st.append(LI("<i>STATS</i>: liczby co sekundę do pliku w RAM:, na ekranie tylko FPS. "
                 "<i>STATS=FULL</i>: sondy E-clock (spowalniają – liczby z nimi tylko do podziału "
                 "czasu). <i>VERIFY</i>: szybka ścieżka porównana ze wzorcową wiersz po wierszu."))
    st.append(LI("Odrzucone pomiary: trzy wyniki (18–32 kl./s) zrobione przez pomyłkę w pauzie "
                 "gry – nie wchodzą do tabel."))

    st.append(P("Etapy portu", "h2"))
    st.append(table(
        ["Faza", "Stan", "Wynik"],
        [["F0 Rozpoznanie", "gotowe", "Freeserf (C++11, GPL-3.0) z danymi Amigi; SDL tylko w 2 plikach poza "
          "warstwą platformy"],
         ["F1 Logika na 68k", "gotowe", "kompilacja bez zmian, deterministyczna; krok z AI 0,6 ms (A1200)"],
         ["F2 Dane na 68k", "gotowe", "2322 sprite'y, 39 dźwięków, 2 moduły; wczytanie 2,7 s, dekodowanie 8,1 s"],
         ["F3 Backend Amigi", "gotowe", "ekran P96 8-bit, mysz, klawiatura, kursor gry, ikona, F9/F10"],
         ["F4 Wydajność", "gotowe*", "9,4 → 46,3 kl./s (Amiga 2); *pomiar na A1200 – gdy będzie dostępna"],
         ["F5 Dźwięk", "gotowe", "Paula 22 kHz, MOD przez micromod, ~5% CPU, 0 przerw"],
         ["F6 Wydanie", "gotowe", "Settlers-0.32.lha, czysta instalacja sprawdzona"]],
        [34, 16, 130]))

    st.append(P("Wydajność obrazu", "h2"))
    st.append(Image(chart, width=180 * mm, height=75 * mm))
    st.append(Spacer(1, 3))
    rows = []
    prev = None
    for v, f, what in FPS[:-1]:
        gain = "" if prev is None else "+%d%%" % round(100 * (f / prev - 1))
        rows.append([v, what, num(f), gain])
        prev = f
    rows.append(["0.22", "muzyka i efekty (miksowanie ~1,1 ms na bufor 23 ms)", "42,4", "−8%"])
    st.append(table(["Wersja", "Zmiana", "kl./s", "względem poprzedniej"], rows,
                    [16, 104, 18, 42], gain_col=3))
    st.append(P("Łącznie 0.09 → 0.20: <b>+358%</b> (4,6 raza szybciej).", "small"))

    st.append(P("Gdzie szedł czas (jedna klatka, sondy)", "h3"))
    st.append(table(
        ["Część klatki", "0.09 (start)", "0.20 (po)", "Co zmieniło"],
        [["Kopie ramek / teren", "40 ms", "~1 ms",
          "GUI rysuje w miejscu; kafelki terenu w VRAM, kopiuje blitter karty (514 MB/s)"],
         ["Sprite'y (~375)", "41 ms", "~16 ms",
          "przebiegi pomiń/kopiuj/cień, cały sprite jedną funkcją, zapis słowami do VRAM"],
         ["Wysłanie na kartę", "10 ms", "0,6 ms", "klatka składana w VRAM, na końcu jeden blit"],
         ["Blokada bitmapy", "–", "~0 ms", "raz na klatkę zamiast ~290 razy (~29 µs każda)"],
         ["Logika gry", "3 ms", "1,5 ms", "bez zmian w logice (inna scena/maszyna)"]],
        [36, 22, 20, 102]))

    st.append(P("Rozdzielczości (0.20, scena gry, Amiga 2)", "h3"))
    st.append(table(["Tryb", "kl./s", "Uwagi"],
                    [["400×300", "~49", "ekran startowy (nie scena gry); najmniejszy tryb, który mieści okna gry"],
                     ["640×400", "49,9", "pułap 50 kl./s"],
                     ["640×480", "46,3", "domyślny"],
                     ["800×600", "32,5", "więcej mapy na ekranie"]],
                    [24, 18, 138]))

    st.append(P("Logika, dane, dźwięk", "h3"))
    st.append(table(["Pomiar", "Wynik", "Maszyna"],
                    [["Krok logiki z AI (30 000 kroków = 10 min gry)", "0,6 ms śr., 13 ms maks.",
                      "A1200 060/50"],
                     ["Powtarzalność (2 przebiegi, ta sama misja)", "identyczne sumy CRC",
                      "A1200 060/50"],
                     ["Wczytanie i dekodowanie danych Amigi", "2,7 s + 8,1 s", "A1200 060/50"],
                     ["Wczytanie zapisu tekstowego Freeserf", "4,6 s", "Amiga 2"],
                     ["Miksowanie dźwięku (bufor 512 próbek, 22 kHz)", "1,1 ms (~5% CPU), 0 przerw",
                      "Amiga 2"]],
                    [86, 52, 42]))

    st.append(PageBreak())
    st.append(P("Błędy znalezione na sprzęcie", "h2"))
    st.append(table(
        ["Wersja", "Objaw", "Przyczyna", "Poprawka"],
        [["0.01", "AmiAgent zawieszony, śmieci w tytule", "SetWindowTitles ustawiało tytuł ekranu na "
          "bufor ze stosu", "argumenty, bufory statyczne"],
         ["0.03", "widać ćwiartkę planszy", "p96BestModeIDTags dawał 320×200 dla 640×480",
          "własny wybór z listy trybów karty"],
         ["0.04", "niebieskie smugi przy wodzie i granicach", "make_transparent: piksel jako uint32 – "
          "błąd big endian", "porównanie bajtów (łatka Freeserf)"],
         ["0.11", "„Freeing memory that is already free” przy wyjściu", "libnix przy Ctrl-C woła exit() "
          "w środku sprzątania – destruktory 2×", "pusta __chkabort()"],
         ["0.15", "szary ekran, Amiga stoi", "paleta zmieniana przy zablokowanej bitmapie karty",
          "blokada zdejmowana przed wywołaniami systemu"],
         ["0.18", "zawieszenie na starcie", "AllocBitMap kafelka przy blokadzie", "j.w."],
         ["0.21", "muzyka zwalnia po 1–2 min", "Fxx ≥ 32 czytane jako BPM; oryginał – tryb VBlank",
          "micromod z MICROMOD_VBLANK"],
         ["0.27", "ostrzeżenia w oknie konsoli na Workbenchu", "dziennik ustawiony po poziomie; brak "
          "dekodera PNG", "kolejność; obrazek wbudowany"]],
        [13, 45, 68, 54]))

    st.append(P("Nowe w wersji na Amigę", "h2"))
    for t in [
        "Ekran Picasso96 8-bit 400×300 / 640×400 / 640×480 / 800×600, zmiana od razu w "
        "<i>Options → Screen</i> (strzałki), zapamiętana.",
        "Muzyka (moduł z dyskietki) i efekty przez Paulę; klawisze M i S.",
        "Kursor z gry; przewijanie myszą i strzałkami.",
        "<i>Options → Advanced</i>: Show FPS; ukryte opcje bez znaczenia na Amidze; okno mieści się "
        "w 400×300.",
        "Start z ikony, Tool Types (FPS, MODE, NOSOUND, DATA), stos w ikonie.",
        "F9 – zrzut ekranu z prawdziwą paletą; F10 – zapis stanu do zgłoszenia błędu.",
        "Narzędzia: simtest, datacheck, modelist, Settlers-hg (strażnik sterty), skrypty AmiAgenta "
        "(pomiar, test wyjścia, restart).",
    ]:
        st.append(LI(t))

    st.append(P("Łatki we Freeserf (gałąź settlers-pin)", "h2"))
    for t in [
        "0001 – zapisy gier w PROGDIR:freeserf (brak HOME na Amidze).",
        "0002 – make_transparent na big endian (smugi).",
        "0003 – GuiObject::use_frame: rysowanie w miejscu (dwie kopie ekranu mniej na klatkę).",
        "0004 – Options → Screen, Advanced → Show FPS, mniejsze okno Advanced.",
    ]:
        st.append(LI(t))
    st.append(P("Reszta portu to osobne pliki (port/): ekran, VRAM, zdarzenia, dźwięk, start z "
                "ikony – kod Freeserf zmieniony w 4 miejscach.", "small"))

    st.append(P("Co dalej", "h2"))
    for t in [
        "Pomiar na A1200 (Warp1260, 060 @ 50 MHz), gdy Amiga 1 będzie dostępna.",
        "Gra sieciowa dla dwóch graczy (okno „Online”, lockstep przez TCP) – osobny etap.",
        "Opcjonalnie AHI (warp.audio); szybsze wczytywanie zapisów (4,6 s).",
    ]:
        st.append(LI(t))

    st.append(PageBreak())
    st.append(P("Zrzuty ekranu (F9, Amiga 2)", "h2"))
    for name, cap, w in [("v1.png", "Scena gry 640×480 (pomiar wydajności)", 150),
                         ("m400.png", "Okno startowe w 400×300 – mieści się w całości", 110)]:
        path = os.path.join(SHOTS, name)
        if os.path.exists(path):
            from PIL import Image as PImage
            iw, ih = PImage.open(path).size
            st.append(KeepTogether([Image(path, width=w * mm, height=w * mm * ih / iw),
                                    P(cap, "small"), Spacer(1, 6)]))

    doc.build(st)
    print(OUT)


if __name__ == "__main__":
    main()
