"""Raport PDF (po polsku): zmiany wersji 0.138 - zasady gry jak w oryginale Amigi.

  python tools/report_0138_pdf.py   ->  docs/Settlers-0.138-raport.pdf

Źródła: docs/ZGODNOSC.md (porównanie z programem z dyskietki 3), docs/PORTING.md
("Zmiany 0.138"), wynik A/B simtest na Amidze 1 (private/rules-ab, liczby wpisane niżej).
"""
import os

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (KeepTogether, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "Settlers-0.138-raport.pdf")

pdfmetrics.registerFont(TTFont("Arial", r"C:\Windows\Fonts\arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", r"C:\Windows\Fonts\arialbd.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Italic", r"C:\Windows\Fonts\ariali.ttf"))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic",
                              boldItalic="Arial-Bold")

BLUE = colors.HexColor("#1f5a9e")
GRID = colors.HexColor("#b0b7c0")
HEAD = colors.HexColor("#dfe7f2")
GOOD = colors.HexColor("#e6f2e6")

ss = {
    "title": ParagraphStyle("t", fontName="Arial-Bold", fontSize=18, leading=22, spaceAfter=2),
    "sub": ParagraphStyle("st", fontName="Arial", fontSize=9.5, leading=13,
                          textColor=colors.HexColor("#444444"), spaceAfter=8),
    "h2": ParagraphStyle("h2", fontName="Arial-Bold", fontSize=12.5, leading=16, spaceBefore=10,
                         spaceAfter=4, textColor=BLUE, keepWithNext=1),
    "h3": ParagraphStyle("h3", fontName="Arial-Bold", fontSize=10, leading=13, spaceBefore=6,
                         spaceAfter=2, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Arial", fontSize=9.3, leading=12.6, spaceAfter=4),
    "li": ParagraphStyle("li", fontName="Arial", fontSize=9.3, leading=12.6, leftIndent=12,
                         bulletIndent=2, spaceAfter=2),
    "cap": ParagraphStyle("cap", fontName="Arial", fontSize=8, leading=10.5,
                          textColor=colors.HexColor("#444444"), spaceBefore=2, spaceAfter=6),
    "cell": ParagraphStyle("c", fontName="Arial", fontSize=8.4, leading=10.6),
    "cellb": ParagraphStyle("cb", fontName="Arial-Bold", fontSize=8.4, leading=10.6),
    "num": ParagraphStyle("n", fontName="Arial", fontSize=8.4, leading=10.6, alignment=2),
}


def P(t, s="p"):
    return Paragraph(t, ss[s])


def LI(t):
    return Paragraph(t, ss["li"], bulletText="•")


def table(header, rows, widths, num_cols=(), hl_col=None):
    data = [[P(h, "cellb") for h in header]]
    for r in rows:
        data.append([P(str(c), "num" if i in num_cols else "cell") for i, c in enumerate(r)])
    t = Table(data, colWidths=widths, repeatRows=1)
    st = [("GRID", (0, 0), (-1, -1), 0.5, GRID), ("BACKGROUND", (0, 0), (-1, 0), HEAD),
          ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("TOPPADDING", (0, 0), (-1, -1), 2.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 2.5)]
    if hl_col is not None:
        st.append(("BACKGROUND", (hl_col, 1), (hl_col, -1), GOOD))
    t.setStyle(TableStyle(st))
    return t


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Arial", 7.5)
    canvas.setFillColor(colors.HexColor("#666666"))
    canvas.drawString(18 * mm, 10 * mm, "Settlers (Freeserf) na Amigę – zmiany 0.138")
    canvas.drawRightString(A4[0] - 18 * mm, 10 * mm, f"str. {doc.page}")
    canvas.restoreState()


def pct(x):
    return f"{100 * x:.0f}%".replace(".", ",")


def fish_rows():
    rows = []
    so = sn = 0.0
    for fish in range(8):
        po = (fish + 4) / 64 if fish else 0.0         # oryginał: (los & 63) < ryby + 4
        pn = max(0, fish - 4) / 64 if fish else 0.0    # 0.137: (los & 63) + 4 < ryby
        wo, wn = 1 - (1 - po) ** 10, 1 - (1 - pn) ** 10
        so += wo
        sn += wn
        if fish in (1, 3, 5, 7):
            rows.append([fish, f"{pct(pn)} / {pct(wn)}", f"{pct(po)} / {pct(wo)}"])
    rows.append(["<b>średnio (0–7)</b>", f"– / <b>{pct(sn / 8)}</b>", f"– / <b>{pct(so / 8)}</b>"])
    return rows


def mine_rows():
    rows = []
    for p in (1.0, 0.5, 0.25, 0.1):
        hit = 1 - (1 - p) ** 4
        rows.append([f"{p:.2f}".replace(".", ","), f"{8 / 7 * hit:.2f}".replace(".", ","),
                     f"{8 * hit:.1f}".replace(".", ",")])
    return rows


AB = [  # suma 4 graczy, z historii resource_count (private/rules-ab/cmp_saves.py)
    ("węgiel", 2, 31), ("ruda żelaza", 2, 17), ("kamień (z kopalni i kamieniarza)", 0, 12),
    ("ruda złota", 0, 1), ("stal", 20, 33), ("deski", 67, 86), ("pnie", 52, 56),
]


def main():
    st = []
    st.append(P("Settlers 0.138 – zasady gry jak w oryginale Amigi", "title"))
    st.append(P("Port Freeserf (The Settlers 1) na Amigę RTG 68040/68060. Wydanie beta 0.138 "
                "z 17.11.2026, łatka Freeserf 0070. Raport zmian, ich skutków i sprawdzenia "
                "na Amidze.", "sub"))

    st.append(P("1. Podsumowanie", "h2"))
    st.append(P("16.11.2026 procedury oryginalnego programu gry z Amigi (<i>TheSettlers</i> z "
                "dyskietki 3) porównano instrukcja po instrukcji z kodem Freeserf, na którym stoi "
                "port (<i>docs/ZGODNOSC.md</i>). Prawie wszystko było zgodne; znalazły się "
                "<b>4 różnice</b>, które zmieniają przebieg gry. Decyzja użytkownika "
                "(17.11.2026): wszystkie na stałe jak w oryginale, bez opcji."))
    st.append(table(
        ["Nr", "Co", "Do 0.137 (Freeserf)", "Od 0.138 (jak oryginał)", "Skutek"],
        [["1", "Jedzenie w kopalniach", "porcja w 7 wejściach górnika na 8",
          "porcja w 1 wejściu na 8 (adres 0xDF56)",
          "kopalnie jedzą ok. 7 razy mniej, kopią dużo więcej"],
         ["2", "Połów ryb", "połów, gdy los + 4 &lt; ryby",
          "połów, gdy los &lt; ryby + 4 (0xDD68)",
          "rybak łowi średnio w 63% wyjść zamiast 10%"],
         ["3", "Przyciski 1 / 5 / 20 / 100 (okno rycerzy)",
          "każdy przycisk zamieniał wszystkich możliwych osadników",
          "najwyżej tylu, ile na przycisku (0x16940)",
          "miecze i tarcze nie znikają naraz"],
         ["4", "Obrońcy zamku przy pełnej obsadzie",
          "zamiana poziomów dwóch obrońców między sobą",
          "najsilniejszy obrońca zamienia się z najsłabszym rycerzem z magazynu zamku "
          "(0xB972–0xBA18)",
          "zamku bronią słabi, silni idą do budynków i do ataku"]],
        [9 * mm, 30 * mm, 38 * mm, 44 * mm, 53 * mm], hl_col=3))

    st.append(P("2. Zmiany po kolei", "h2"))

    st.append(P("2.1 Jedzenie w kopalniach", "h3"))
    st.append(P("Przy każdym wejściu do kopalni gra losuje liczbę 0–7. W oryginale górnik bierze "
                "porcję jedzenia (chleb, ryba lub mięso) tylko przy 0, czyli w 1 wejściu na 8; "
                "w pozostałych kopie bez jedzenia. Freeserf miał to odwrotnie. Gdy wypada wejście "
                "z jedzeniem, a zapasu nie ma, górnik czeka – tak samo w obu wersjach. Czasy "
                "kopania się nie zmieniły (do 4 prób po 1000 tików, ok. 27 s na urobek przy "
                "bogatym złożu)."))
    st.append(KeepTogether([
        table(["Złoże (część z 32 pól wokół kopalni)", "Urobek na porcję: 0.137",
               "Urobek na porcję: 0.138"], mine_rows(), [62 * mm, 50 * mm, 50 * mm],
              num_cols=(1, 2), hl_col=2),
        P("Urobek na porcję = (8/7 albo 8) · (1 − (1 − p)<super>4</super>), p – część pól ze "
          "złożem. Zasięg kopalni opisuje osobny PDF <i>docs/Settlers-kopalnie-zasieg.pdf</i>.",
          "cap")]))

    st.append(P("2.2 Połów ryb", "h3"))
    st.append(P("Ryb na polu wody jest 0–7. Rybak robi do 10 prób na jedno wyjście. W oryginale "
                "próba udaje się, gdy los (0–63) jest mniejszy niż liczba ryb + 4; we Freeserf "
                "warunek był przestawiony (los + 4 &lt; ryby), więc woda z 1–4 rybami nigdy nic "
                "nie dawała. Przy 0 rybach rybak nie próbuje w obu wersjach."))
    st.append(KeepTogether([
        table(["Ryb na polu", "0.137: próba / wyjście", "0.138: próba / wyjście"], fish_rows(),
              [40 * mm, 60 * mm, 60 * mm], num_cols=(1, 2), hl_col=2),
        P("Wyjście = do 10 prób: 1 − (1 − p)<super>10</super>.", "cap")]))

    st.append(P("2.3 Przyciski „szkól 1 / 5 / 20 / 100” w oknie rycerzy", "h3"))
    st.append(P("Przycisk zamienia bezczynnych osadników z magazynów w rycerzy (każdy zużywa "
                "miecz i tarczę). W oryginale pętla kończy się, gdy licznik z przycisku dojdzie "
                "do zera. We Freeserf licznik malał, ale nic nie zatrzymywał – każdy przycisk "
                "zużywał wszystkie miecze i tarcze naraz. Ta sama funkcja służy komputerowemu "
                "przeciwnikowi; jego liczba pochodzi z oryginału, więc teraz też działa jak tam."))

    st.append(P("2.4 Obrońcy zamku", "h3"))
    st.append(P("Zamek chce tylu obrońców, ile ustawiono w oknie rycerzy (domyślnie 3). Gdy jest "
                "ich dokładnie tyle, oryginał w każdym kroku budynku:"))
    st.append(LI("szuka <b>najsilniejszego obrońcy</b> (pierwszego o najwyższym poziomie),"))
    st.append(LI("szuka w magazynie zamku wolnego rycerza o niższym poziomie, od poziomu 0 w górę,"))
    st.append(LI("jeśli taki jest – <b>zamienia ich poziomy</b>: silny trafia do magazynu, słaby "
                 "do obrony."))
    st.append(P("Freeserf zamieniał tylko poziomy najsłabszego i ostatniego obrońcy między sobą, "
                "bez magazynu. W zamku zostawali więc rycerze, którzy weszli do obrony (zwykle "
                "najsilniejsi). Teraz silni rycerze czekają w magazynie zamku i stamtąd idą do "
                "budynków wojskowych i do ataku. Szkolenie obrońców i rycerzy w magazynie zamku "
                "jest takie samo (progi 4000 / 2000 / 1000 / 500)."))

    st.append(P("3. Sprawdzenie na Amidze", "h2"))
    st.append(P("Porównanie A/B na <b>Amidze 1</b> (A1200, Warp 1260, 68060): program testowy "
                "<i>simtest</i> (sama logika gry, bez obrazu) w wersji 0.137 i 0.138, ta sama "
                "losowa mapa (rozmiar 3) z 4 graczami komputerowymi, to samo ziarno losowania, "
                "100 000 kroków = 200 000 tików (ok. 33 min gry w normalnym tempie). Liczby to "
                "wyprodukowane towary z historii statystyk gry, suma 4 graczy."))
    st.append(table(["Towar", "0.137", "0.138"], [[n, a, b] for n, a, b in AB],
                    [70 * mm, 30 * mm, 30 * mm], num_cols=(1, 2), hl_col=2))
    st.append(Spacer(1, 4))
    st.append(table(["Inne", "0.137", "0.138"], [
        ["punkty za budynki (suma)", 306, 362],
        ["punkty wojskowe (suma)", 808, 832],
        ["rycerze poziom 0 / 1 / 2 / 3", "168 / 202 / 43 / 8", "202 / 197 / 55 / 2"],
        ["czas logiki na tik (Amiga 1)", "2,17 ms", "2,04 ms"],
    ], [70 * mm, 40 * mm, 40 * mm], num_cols=(1, 2)))
    st.append(Spacer(1, 4))
    st.append(LI("Kopalnie wydobywają kilka razy więcej (węgiel 2 → 31, ruda żelaza 2 → 17), "
                 "więc huty robią więcej stali, a cała gospodarka rośnie szybciej."))
    st.append(LI("Na tej mapie gracze komputerowi nie łowili ryb – zmiana 2 wynika z kodu i "
                 "tabeli w 2.2, test jej nie mierzył. Zmian 3 i 4 też nie widać w tych liczbach "
                 "(komputer nie używa przycisków, a obsady zamku statystyki nie pokazują)."))
    st.append(LI("Wydanie: paczka publiczna rozpakowana <i>UnZip</i> w RAM: na Amidze 1, "
                 "<i>Version</i> = 0.138, gra startuje do okna startowego. Na Amidze 1 "
                 "<i>WorkSD:Games/Settlers</i> to teraz 0.138 (poprzednia 0.136 w <i>old/</i>)."))

    st.append(P("4. Co to znaczy dla gracza", "h2"))
    st.append(LI("Kopalnie potrzebują dużo mniej jedzenia – rybacy, rzeźnie i piekarnie "
                 "wystarczą na więcej kopalń."))
    st.append(LI("Rybak daje jedzenie z każdej wody z rybami."))
    st.append(LI("Przycisk 1 / 5 / 20 / 100 robi dokładnie tylu rycerzy (o ile są miecze, tarcze "
                 "i wolni osadnicy)."))
    st.append(LI("Najsilniejsi rycerze nie siedzą w zamku – są w jego magazynie i idą tam, gdzie "
                 "są potrzebni."))
    st.append(LI("Zapisy ze starszych wersji wczytują się i grają dalej według nowych zasad."))
    st.append(LI("Gra LAN tylko wtedy, gdy obie strony mają 0.138."))

    st.append(P("5. Pliki", "h2"))
    st.append(table(["Co", "Gdzie"], [
        ["kod (Freeserf)", "<i>serf.cc</i> (kopalnie, rybak), <i>player.cc</i> (przyciski), "
                           "<i>building.cc</i> (obrońcy zamku)"],
        ["łatka", "<i>port/freeserf-patches/0070-0.138-game-rules-as-the-Amiga-original…</i>"],
        ["porównanie z oryginałem", "<i>docs/ZGODNOSC.md</i>; poprzedni raport "
                                    "<i>docs/Settlers-0.137-raport.pdf</i>"],
        ["opis zmian", "<i>docs/PORTING.md</i>, „Zmiany 0.138”; historia wydań "
                       "<i>Settlers_release.txt</i> / <i>_en.txt</i>"],
        ["paczki", "<i>releases/amiga/Settlers-0.138.zip</i> (bez danych gry), "
                   "<i>private/Settlers-0.138-full.zip</i>"],
        ["zasięg kopalni", "<i>docs/Settlers-kopalnie-zasieg.pdf</i> "
                           "(<i>tools/mine_area_pdf.py</i>)"],
    ], [40 * mm, 134 * mm]))

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                            topMargin=16 * mm, bottomMargin=18 * mm,
                            title="Settlers 0.138 – zmiany")
    doc.build(st, onFirstPage=footer, onLaterPages=footer)
    print(OUT)


if __name__ == "__main__":
    main()
