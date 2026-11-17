"""PDF: zasięg kopalni w Settlers (Freeserf, serf.cc handle_serf_mining_state).

usage: python tools/mine_area_pdf.py docs/Settlers-kopalnie-zasieg.pdf

Pola liczone jak w grze: map.cc spiral_pattern, indeksy 0..31 ((los >> 2) & 0x1f).
Współrzędne Freeserf: sąsiedzi (1,0) prawo, (1,1) prawo-dół, (0,1) lewo-dół,
(-1,0) lewo, (-1,-1) lewo-góra, (0,-1) prawo-góra. Flaga budynku = (1,1).
"""
import sys

from reportlab.graphics.shapes import Circle, Drawing, String
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (KeepTogether, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

OUT = sys.argv[1]

pdfmetrics.registerFont(TTFont("Arial", r"C:\Windows\Fonts\arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", r"C:\Windows\Fonts\arialbd.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Italic", r"C:\Windows\Fonts\ariali.ttf"))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic",
                              boldItalic="Arial-Bold")

BLUE = colors.HexColor("#1f5a9e")
GRID = colors.HexColor("#b0b7c0")
HEAD = colors.HexColor("#dfe7f2")
RING = {0: colors.HexColor("#333333"), 1: colors.HexColor("#c0392b"),
        2: colors.HexColor("#e67e22"), 3: colors.HexColor("#f1c40f")}
RING2 = colors.HexColor("#5dade2")
SHARED = colors.HexColor("#8e44ad")
EMPTY = colors.HexColor("#ffffff")

ss = {
    "title": ParagraphStyle("t", fontName="Arial-Bold", fontSize=18, leading=22, spaceAfter=2),
    "sub": ParagraphStyle("st", fontName="Arial", fontSize=9.5, leading=13,
                          textColor=colors.HexColor("#444444"), spaceAfter=8),
    "h2": ParagraphStyle("h2", fontName="Arial-Bold", fontSize=12.5, leading=16, spaceBefore=9,
                         spaceAfter=4, textColor=BLUE, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Arial", fontSize=9.5, leading=13, spaceAfter=4),
    "li": ParagraphStyle("li", fontName="Arial", fontSize=9.5, leading=13, leftIndent=12,
                         bulletIndent=2, spaceAfter=2),
    "cap": ParagraphStyle("cap", fontName="Arial", fontSize=8, leading=10.5,
                          textColor=colors.HexColor("#444444"), spaceAfter=6),
    "cell": ParagraphStyle("c", fontName="Arial", fontSize=9, leading=11),
    "cellb": ParagraphStyle("cb", fontName="Arial-Bold", fontSize=9, leading=11),
}


def P(t, s="p"):
    return Paragraph(t, ss[s])


def LI(t):
    return Paragraph(t, ss["li"], bulletText="•")


# --- spirala z map.cc (pierwsze 6 wierszy wzoru = indeksy 0..36) ---------------------------
BASE = [(1, 0), (2, 1), (2, 0), (3, 1), (3, 2), (3, 0)]
ROT = [(1, 0, 0, 1), (1, 1, -1, 0), (0, 1, -1, -1), (-1, 0, 0, -1), (-1, -1, 1, 0),
       (0, -1, 1, 1)]


def spiral():
    pts = [(0, 0)]
    for x, y in BASE:
        for a, b, c, d in ROT:
            pts.append((x * a + y * c, x * b + y * d))
    return pts


AREA = spiral()[:32]          # (los >> 2) & 0x1f -> indeksy 0..31
AREA_SET = set(AREA)


def dist(dx, dy):
    if (dx >= 0) == (dy >= 0):
        return max(abs(dx), abs(dy))
    return abs(dx) + abs(dy)


def reach(mine, others):
    """Pola, z których górnik kopalni `mine` może kopać (bez budynków i flag)."""
    blocked = set()
    for m in [mine] + others:
        blocked.add(m)
        blocked.add((m[0] + 1, m[1] + 1))   # flaga
    return {(mine[0] + x, mine[1] + y) for x, y in AREA} - blocked


# --- rysunek siatki ------------------------------------------------------------------------
W = 17.0          # odstęp punktów w rzędzie
H = 14.7          # odstęp rzędów


def scr(p, ox, oy):
    c, r = p
    return ox + (c - r / 2.0) * W, oy - r * H


def grid_points(cmin, cmax, rmin, rmax):
    return [(c, r) for r in range(rmin, rmax + 1) for c in range(cmin + (r + 1) // 2 - 1,
                                                                    cmax + r // 2 + 1)]


def draw_single():
    d = Drawing(330, 150)
    ox, oy = 165 - 0 * W, 120
    pts = [p for p in grid_points(-5, 5, -4, 4) if dist(*p) <= 4]
    for p in pts:
        x, y = scr(p, ox, oy - 45)
        k = dist(*p)
        if p == (0, 0):
            fill, label, fc = RING[0], "K", colors.white
        elif p == (1, 1):
            fill, label, fc = colors.HexColor("#7f8c8d"), "F", colors.white
        elif p in AREA_SET:
            fill, label, fc = RING[k], str(k), colors.black
        elif k == 3:
            fill, label, fc = EMPTY, "x", colors.HexColor("#888888")
        else:
            fill, label, fc = EMPTY, "", colors.black
        d.add(Circle(x, y, 6.4, fillColor=fill, strokeColor=GRID, strokeWidth=0.6))
        if label:
            d.add(String(x, y - 3, label, fontName="Arial-Bold", fontSize=7.5,
                         fillColor=fc, textAnchor="middle"))
    return d


def draw_pair(dc, dr, width=420):
    m1, m2 = (0, 0), (dc, dr)
    r1, r2 = reach(m1, [m2]), reach(m2, [m1])
    pts = [p for p in grid_points(-5, dc + 5, -4, dr + 4)
           if dist(*p) <= 4 or dist(p[0] - dc, p[1] - dr) <= 4]
    xs = [scr(p, 0, 0)[0] for p in pts]
    ys = [scr(p, 0, 0)[1] for p in pts]
    ox = (width - (max(xs) - min(xs))) / 2 - min(xs)
    hgt = max(ys) - min(ys) + 20
    oy = hgt - 10 - max(ys)
    d = Drawing(width, hgt)
    for p in pts:
        x, y = scr(p, ox, oy)
        label, fc = "", colors.black
        if p in (m1, m2):
            fill, fc = RING[0], colors.white
            label = "K1" if p == m1 else "K2"
        elif p in ((1, 1), (dc + 1, dr + 1)):
            fill, label, fc = colors.HexColor("#7f8c8d"), "F", colors.white
        elif p in r1 and p in r2:
            fill = SHARED
        elif p in r1:
            fill = RING[2]
        elif p in r2:
            fill = RING2
        else:
            fill = EMPTY
        d.add(Circle(x, y, 6.4, fillColor=fill, strokeColor=GRID, strokeWidth=0.6))
        if label:
            d.add(String(x, y - 3, label, fontName="Arial-Bold", fontSize=6.5 if len(label) > 1
                         else 7.5, fillColor=fc, textAnchor="middle"))
    return d, len(r1 & r2), len(r1), len(r2)


def legend(items):
    d = Drawing(460, 16)
    x = 4
    for col, txt in items:
        d.add(Circle(x + 6, 8, 5.5, fillColor=col, strokeColor=GRID, strokeWidth=0.6))
        d.add(String(x + 16, 5, txt, fontName="Arial", fontSize=8))
        x += 16 + pdfmetrics.stringWidth(txt, "Arial", 8) + 14
    return d


def table(header, rows, widths):
    data = [[P(h, "cellb") for h in header]] + [[P(str(c), "cell") for c in r] for r in rows]
    t = Table(data, colWidths=widths)
    t.setStyle(TableStyle([
        ("GRID", (0, 0), (-1, -1), 0.5, GRID),
        ("BACKGROUND", (0, 0), (-1, 0), HEAD),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("TOPPADDING", (0, 0), (-1, -1), 2.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 2.5),
    ]))
    return t


def main():
    st = []
    st.append(P("Zasięg kopalni w Settlers", "title"))
    st.append(P("Ile pól wokół kopalni przeszukuje górnik i co się dzieje, gdy kopalnie stoją "
                "blisko siebie. Port Freeserf na Amigę, zasady jak w oryginale z Amigi "
                "(<i>docs/ZGODNOSC.md</i>). Źródło: <i>freeserf/src/serf.cc</i> "
                "(<i>handle_serf_mining_state</i>), <i>map.cc</i> (spirala pól).", "sub"))

    st.append(P("1. Co to jest „pole”", "h2"))
    st.append(P("Mapa Settlers to siatka punktów. Każdy punkt ma 6 sąsiadów: z lewej, z prawej "
                "i po dwóch u góry i u dołu. Na jednym punkcie stoi jeden budynek, jedna flaga "
                "albo jedno drzewo; pod punktem może leżeć złoże (złoto, żelazo, węgiel, kamień). "
                "„Krok” to przejście do sąsiedniego punktu."))
    st.append(P("Na ekranie sąsiednie punkty w rzędzie są <b>32 piksele</b> od siebie, kolejne "
                "rzędy <b>20 pikseli</b> niżej (każdy przesunięty o pół punktu); na ekranie "
                "640 pikseli szerokości mieści się 20 punktów w rzędzie. W górach rzędy "
                "przesuwają się z wysokością terenu. W grze: mały budynek (kopalnia) stoi na "
                "jednym punkcie, jego flaga na sąsiednim (prawo-dół); jeden odcinek drogi między "
                "sąsiednimi punktami to 1 krok."))

    st.append(P("2. Obszar, z którego kopie górnik", "h2"))
    st.append(KeepTogether([
        draw_single(),
        legend([(RING[0], "K – kopalnia"), (colors.HexColor("#7f8c8d"), "F – jej flaga"),
                (RING[1], "1 krok"), (RING[2], "2 kroki"), (RING[3], "3 kroki"),
                (EMPTY, "x / puste – poza zasięgiem")]),
        P("Rys. 1. 32 punkty przeszukiwane przez górnika (z kopalnią). Narożniki „x” "
          "3. pierścienia nie należą do obszaru – poza jednym, prawym.", "cap"),
    ]))
    st.append(table(["Odległość od kopalni", "Punktów", "Uwagi"], [
        ["0 (sama kopalnia)", "1", "zawsze zajęty przez kopalnię – nie do wykopania"],
        ["1 krok", "6", "w tym flaga kopalni (prawo-dół) – nie do wykopania"],
        ["2 kroki", "12", "wszystkie"],
        ["3 kroki", "13 z 18", "bez 5 narożników (prawy narożnik jest)"],
        ["<b>Razem</b>", "<b>32</b>", "<b>30</b> możliwych do wykopania, gdy obok nic nie stoi"],
    ], [42 * mm, 22 * mm, 106 * mm]))
    st.append(Spacer(1, 6))
    st.append(P("Obszar ma kształt plastra miodu: 7 punktów szerokości i 7 rzędów wysokości, "
                "z kopalnią w środku. Górnik sięga najwyżej 3 kroki od kopalni."))

    st.append(P("3. Jak górnik kopie", "h2"))
    st.append(LI("Przy każdym wejściu do kopalni górnik robi do <b>4 prób</b> (po 1000 tików)."))
    st.append(LI("W każdej próbie losuje jeden z 32 punktów, każdy z tą samą szansą 1/32."))
    st.append(LI("Próba się udaje, gdy pod punktem jest złoże <b>właściwego rodzaju</b> i na "
                 "punkcie <b>nie stoi budynek ani flaga</b> (drzewa, kamienie, drogi nie "
                 "przeszkadzają). Ze złoża ubywa 1 jednostka, górnik wynosi urobek."))
    st.append(LI("Gdy żadna z 4 prób nie trafi, górnik wychodzi bez urobku i zaczyna od nowa. "
                 "Im mniej punktów ze złożem w obszarze, tym częściej wraca z niczym – "
                 "kopalnia „wysycha” stopniowo."))
    st.append(table(["Punktów ze złożem (z 32)", "Szansa na urobek w jednym wejściu"],
                    [[n, f"{100 * (1 - (1 - n / 32) ** 4):.0f} %"]
                     for n in (30, 20, 10, 5, 2, 1)], [60 * mm, 70 * mm]))

    st.append(P("4. Kopalnie obok siebie", "h2"))
    st.append(P("Gra <b>nie sprawdza</b>, czy punkt należy do obszaru innej kopalni, ani czyj "
                "jest teren (granica też nie). Jeśli obszary dwóch kopalń się nakładają, obie "
                "kopią ze wspólnych punktów: kto pierwszy wylosuje punkt, ten zabiera rudę, a "
                "drugi górnik trafia potem na pusty punkt. Każda kopalnia zabiera za to sąsiadce "
                "2 punkty: ten, na którym stoi, i ten pod swoją flagą."))
    d4, s4, a4, b4 = draw_pair(4, 0)
    st.append(KeepTogether([
        d4,
        legend([(RING[2], "tylko K1"), (RING2, "tylko K2"), (SHARED, "wspólne – kopią obie"),
                (colors.HexColor("#7f8c8d"), "F – flagi")]),
        P(f"Rys. 2. Dwie kopalnie w jednym rzędzie, 4 kroki od siebie: {s4} wspólnych punktów, "
          f"każda ma {a4} punktów do kopania.", "cap"),
    ]))
    d2, s2, a2, b2 = draw_pair(2, 2)
    st.append(KeepTogether([
        d2,
        P(f"Rys. 3. Kopalnie po skosie (prawo-dół), 2 kroki od siebie: {s2} wspólnych punktów; "
          f"K1 ma {a2}, K2 {b2} punktów do kopania.", "cap"),
    ]))

    worst = {}
    for dc in range(-8, 9):
        for dr in range(-8, 9):
            k = dist(dc, dr)
            if k:
                sh = len(reach((0, 0), [(dc, dr)]) & reach((dc, dr), [(0, 0)]))
                worst[k] = max(worst.get(k, 0), sh)
    rows = []
    for k in range(1, 8):
        _, sh, _, _ = draw_pair(k, 0)
        _, sd, _, _ = draw_pair(k, k)
        _, su, _, _ = draw_pair(0, k)
        rows.append([f"{k}", sh, sd, su, f"<b>{worst.get(k, 0)}</b>"])
    st.append(table(["Odległość (kroki)", "Wspólne punkty: w rzędzie", "po skosie prawo-dół",
                     "po skosie lewo-dół", "najwięcej (dowolne ustawienie)"], rows,
                    [28 * mm, 34 * mm, 34 * mm, 34 * mm, 40 * mm]))
    st.append(Spacer(1, 6))
    st.append(P("<b>Wniosek:</b> kopalnie odległe o 7 kroków i więcej mają zawsze osobne obszary. "
                "Bliżej dzielą złoże – opłaca się to tylko wtedy, gdy złoże jest małe i chcemy "
                "wykopać je szybciej dwiema kopalniami naraz."))

    st.append(P("5. Kopalnia przy granicy", "h2"))
    st.append(P("Granica nie ogranicza górnika. Kopalnię i jej flagę trzeba postawić na własnym "
                "terenie, ale górnik kopie ze wszystkich 32 punktów, także za granicą – na ziemi "
                "niczyjej i na terenie przeciwnika."))
    st.append(LI("Za granicą przeszkadza tylko budynek albo flaga przeciwnika (z takiego punktu "
                 "nie da się kopać); pod drogą, drzewem i kamieniem górnik kopie normalnie."))
    st.append(LI("Kopalnia przeciwnika tego samego rodzaju po drugiej stronie granicy, z "
                 "nakładającym się obszarem, kopie z tych samych punktów – zabieracie sobie rudę "
                 "nawzajem, jak dwie własne kopalnie (rozdział 4)."))
    st.append(LI("Kopalnia przy granicy może więc wykopać złoże, którego nie widać jako własne, "
                 "i wyczerpać złoże, które przeciwnik ma u siebie."))

    st.append(P("6. Złoże pod kopalnią, pod flagą i po zburzeniu", "h2"))
    st.append(P("Złoże ubywa w grze <b>tylko wtedy, gdy górnik wykopie jednostkę</b> (1 na "
                "udaną próbę). Postawienie budynku, flagi czy drogi niczego nie niszczy."))
    st.append(LI("<b>Kopalnia na znaczku geologa:</b> budynek zastępuje znaczek, ruda pod spodem "
                 "zostaje cała. Kopalnia nie kopie spod siebie ani spod swojej flagi, więc te "
                 "2 punkty zostają nietknięte."))
    st.append(LI("<b>Kopalnia obok drugiej:</b> póki druga kopalnia stoi, nikt nie wykopie rudy "
                 "spod niej ani spod jej flagi – ani jej górnik, ani sąsiedni."))
    st.append(LI("<b>Po zburzeniu:</b> gdy budynek się dopali, punkt jest pusty i sąsiednia "
                 "kopalnia (do 3 kroków) może kopać rudę spod miejsca zburzonej kopalni. Flaga "
                 "po zburzeniu zostaje – rudę spod niej odblokowuje dopiero usunięcie flagi."))
    st.append(P("Przykład: K1 i K2 stoją 2 kroki od siebie. Póki stoją obie, K1 nie kopie spod K2 "
                "ani spod flagi K2. Po zburzeniu K2 i usunięciu jej flagi oba punkty trafiają do "
                "zasięgu K1 i K1 je wykopie."))
    st.append(Spacer(1, 4))
    st.append(P("Wszystkie zasady z tego opisu są takie same w oryginale na Amigę "
                "(porównanie z programem z dyskietki w <i>docs/ZGODNOSC.md</i>).", "cap"))

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                            topMargin=16 * mm, bottomMargin=16 * mm,
                            title="Zasięg kopalni w Settlers")
    doc.build(st)
    print(OUT)


if __name__ == "__main__":
    main()
