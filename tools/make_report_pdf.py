"""Raport PDF (po polsku): różnice i błędy wersji 0.137 względem oryginału Amigi
oraz opisane okna gry. Źródła: docs/ZGODNOSC.md, docs/OKNO_*.md, docs/shots/*-opis.png.

  python tools/make_report_pdf.py   ->  docs/Settlers-0.137-raport.pdf
"""
import os

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "Settlers-0.137-raport.pdf")
SHOTS = os.path.join(ROOT, "docs", "shots")

pdfmetrics.registerFont(TTFont("Arial", "C:/Windows/Fonts/arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", "C:/Windows/Fonts/arialbd.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Italic", "C:/Windows/Fonts/ariali.ttf"))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold",
                              italic="Arial-Italic", boldItalic="Arial-Bold")

BASE = ParagraphStyle("base", fontName="Arial", fontSize=9.5, leading=12.5,
                      alignment=TA_LEFT, spaceAfter=4)
SMALL = ParagraphStyle("small", parent=BASE, fontSize=8.3, leading=10.5, spaceAfter=0)
SMALLB = ParagraphStyle("smallb", parent=SMALL, fontName="Arial-Bold")
H0 = ParagraphStyle("h0", parent=BASE, fontName="Arial-Bold", fontSize=20, leading=24,
                    spaceAfter=6)
H1 = ParagraphStyle("h1", parent=BASE, fontName="Arial-Bold", fontSize=14, leading=17,
                    spaceBefore=10, spaceAfter=6, textColor=colors.HexColor("#7a2a00"))
H2 = ParagraphStyle("h2", parent=BASE, fontName="Arial-Bold", fontSize=11.5, leading=14,
                    spaceBefore=8, spaceAfter=4)
NOTE = ParagraphStyle("note", parent=BASE, fontSize=8.5, leading=11,
                      textColor=colors.HexColor("#555555"))

RED = colors.HexColor("#c81414")
HEAD = colors.HexColor("#e9dccb")
GRID = colors.HexColor("#b9a58c")


def P(t, s=BASE):
    return Paragraph(t, s)


def table(rows, widths, head=True, bad_rows=()):
    data = [[P(c, SMALLB if (head and i == 0) else SMALL) for c in r]
            for i, r in enumerate(rows)]
    t = Table(data, colWidths=[w * cm for w in widths], repeatRows=1 if head else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.4, GRID),
          ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3),
          ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]
    if head:
        st.append(("BACKGROUND", (0, 0), (-1, 0), HEAD))
    for r in bad_rows:
        st.append(("BACKGROUND", (0, r), (-1, r), colors.HexColor("#fbe3e0")))
    t.setStyle(TableStyle(st))
    return t


def shot(name, width_cm=17.5):
    path = os.path.join(SHOTS, name)
    img = Image(path)
    ratio = img.imageHeight / float(img.imageWidth)
    img.drawWidth = width_cm * cm
    img.drawHeight = width_cm * cm * ratio
    return img


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Arial", 8)
    canvas.setFillColor(colors.HexColor("#777777"))
    canvas.drawString(1.8 * cm, 1.1 * cm,
                      "Settlers (Freeserf) na Amigę – raport zgodności z oryginałem, wersja 0.137")
    canvas.drawRightString(A4[0] - 1.8 * cm, 1.1 * cm, "strona %d" % doc.page)
    canvas.restoreState()


s = []
s.append(P("Settlers na Amigę – różnice i błędy względem oryginału", H0))
s.append(P("Wersja portu <b>0.137</b> (Freeserf), 16.11.2026. Porównanie z oryginalnym programem "
           "gry z Amigi oraz opis okien gry z numerami przycisków.", BASE))
s.append(Spacer(1, 4))
s.append(P("<b>Jak sprawdzano.</b> Program <i>TheSettlers</i> z dyskietki 3 oryginału (192 688 B) "
           "zdisasemblowany (<i>m68k-amigaos-objdump</i>). Tablica stanów osadnika w oryginale "
           "(adres 0xBFF4) ma tę samą numerację co Freeserf, więc każdą procedurę gry porównano "
           "instrukcja po instrukcji z odpowiednią funkcją portu. Funkcja losująca oryginału jest "
           "identyczna z Freeserf. Adresy w raporcie to miejsca w kodzie oryginału. Wersji DOS "
           "(z której powstał Freeserf) nie mamy.", BASE))

# ---------------------------------------------------------------- podsumowanie
s.append(P("1. Podsumowanie", H1))
s.append(P("Znalezione <b>4 różnice</b>, które zmieniają przebieg gry:", BASE))
s.append(table([
    ["Nr", "Co", "Oryginał Amigi", "Port 0.137 (Freeserf)", "Skutek dla gracza"],
    ["1", "Jedzenie w kopalniach", "porcja w 1 wejściu górnika na 8",
     "porcja w 7 wejściach na 8",
     "w porcie kopalnie jedzą ok. 7 razy więcej; bogate złoże: 8 urobku z porcji w oryginale, 1,14 w porcie"],
    ["2", "Połów ryb", "połów, gdy los (0–63) < ryby + 4", "połów, gdy los + 4 < ryby",
     "rybak łowi średnio w 63% wyjść w oryginale, w 10% w porcie; woda z 1–4 rybami w porcie nic nie daje"],
    ["3", "Przyciski szkól 1 / 5 / 20 / 100 (okno rycerzy)",
     "zamienia najwyżej tylu osadników, ile na przycisku",
     "zamienia wszystkich możliwych (błąd: licznik bez wpływu)",
     "każdy przycisk zużywa naraz wszystkie miecze i tarcze"],
    ["4", "Obrońcy zamku",
     "najsilniejszy obrońca zamienia się z najsłabszym rycerzem z magazynu zamku – w zamku zostają najsłabsi",
     "zamiana tylko między dwoma obrońcami, bez magazynu (błąd przepisania)",
     "w porcie silni rycerze zostają bezczynnie w zamku; w oryginale idą do budynków i do ataku"],
], [0.8, 2.8, 4.3, 4.1, 5.5], bad_rows=(1, 2, 3, 4)))
s.append(Spacer(1, 6))
s.append(P("<b>Drobne, bez wpływu na rozgrywkę:</b> leśniczy wybiera sosnę lub drzewo liściaste "
           "w oryginale z pozycji promienia ekranu (rejestr $DFF007), w porcie z generatora gry "
           "(w obu 50/50, inny przebieg losowania); zaokrąglenia przy wyborze narzędzia i przy "
           "morale (różnica o ułamek procenta).", BASE))
s.append(P("<b>Zgodne z oryginałem</b> (sprawdzone): młyn, piekarnia, chlewnia, rzeźnia, huty żelaza "
           "i złota, zbrojownia, narzędziownia, stocznia, tartak, kamieniarz, drwal, rolnik, "
           "kopalnie (poza jedzeniem), złoto w strażnicach, morale rycerzy, place budowy, domyślne "
           "priorytety gracza, szkolenie rycerzy, walka, zamiana osadników w rycerzy, geolog, "
           "tablice i decyzje transportu, magazyny, obsada budynków wojskowych.", BASE))
s.append(P("Różnice 1 i 2 mogą pochodzić z wersji DOS (nie da się tego sprawdzić bez niej); "
           "3 i 4 to błędy przepisania we Freeserf. Każdą poprawia się kilkoma liniami kodu; "
           "zmieniają przebieg gry (stare zapisy, gra LAN – obie strony ta sama wersja). "
           "<b>Do decyzji:</b> na stałe jak oryginał Amigi albo jako opcja w Advanced.", NOTE))

# ---------------------------------------------------------------- różnice
s.append(PageBreak())
s.append(P("2. Różnice szczegółowo", H1))

s.append(P("Różnica 1 – kopalnie zużywają w oryginale ok. 7 razy mniej jedzenia", H2))
s.append(P("Górnik w każdym wejściu: odpoczynek 100–611 tików, jedzenie, do 4 prób kopania po "
           "1000 tików (losowe pole z 32 wokół kopalni), urobek na flagę (100 tików = 1 s przy "
           "normalnym tempie). Oryginał (0xDFAA): <i>los &amp; 7 ≠ 0</i> (7 na 8) – bez jedzenia; "
           "<i>= 0</i> (1 na 8) – bierze porcję, a gdy zapas pusty, czeka. Freeserf ma to odwrotnie.", BASE))
s.append(table([
    ["Złoże wokół kopalni (część pól)", "Oryginał: urobek z 1 porcji", "Port: urobek z 1 porcji"],
    ["bogate (100%)", "8,0", "1,14"], ["połowa (50%)", "7,5", "1,07"],
    ["ćwierć (25%)", "5,5", "0,78"], ["10%", "2,8", "0,39"],
], [6, 5, 5]))
s.append(P("Bez jedzenia kopalnia w oryginale kopie dalej, aż trafi wejście „z jedzeniem” "
           "(średnio po ~7), wtedy staje. Czasy kopania są takie same.", NOTE))

s.append(P("Różnica 2 – rybacy łowią w oryginale dużo częściej", H2))
s.append(P("Ryb na polu wody jest 0–7. Rybak ma do 10 prób na wyjście. Oryginał (0xDD6E): połów, "
           "gdy <i>los &lt; ryby + 4</i>; Freeserf: gdy <i>los + 4 &lt; ryby</i>.", BASE))
s.append(table([
    ["Ryb na polu", "Oryginał: próba / wyjście", "Port: próba / wyjście"],
    ["1", "7,8% / 56%", "0% / 0%"], ["3", "10,9% / 69%", "0% / 0%"],
    ["5", "14,1% / 78%", "1,6% / 15%"], ["7", "17,2% / 85%", "4,7% / 38%"],
    ["średnio (0–7)", "63% wyjść z rybą", "10% wyjść z rybą"],
], [4, 6, 6]))

s.append(P("Różnica 3 – przyciski „szkól 1 / 5 / 20 / 100”", H2))
s.append(P("Oryginał (0x16940): pętla po wolnych osadnikach w magazynach; każdy z mieczem i tarczą "
           "zostaje rycerzem, licznik z przycisku −1, przy zerze koniec. Freeserf "
           "(<i>Player::promote_serfs_to_knights</i>) zmniejsza licznik, ale się nie zatrzymuje – "
           "każdy przycisk zamienia wszystkich możliwych osadników. Przyciski: okno rycerzy, "
           "numery 4–7 (rozdział 3).", BASE))

s.append(P("Różnica 4 – obrońcy zamku", H2))
s.append(P("Zamek chce tylu obrońców, ile ustawiono w oknie rycerzy (16 / 19, domyślnie 3, od 1 do 99). "
           "Oryginał 0xB824, port <i>Building::update_castle</i>.", BASE))
s.append(table([
    ["Sytuacja", "Oryginał Amigi", "Port 0.137"],
    ["obrońców za mało", "z magazynu zamku najsilniejszy wolny rycerz (poz. 4 → 0); gdy brak – nowy "
     "rycerz z wolnego osadnika (miecz + tarcza); gdy i to nie – wezwanie z innego magazynu", "to samo"],
    ["obrońców za dużo", "pierwszy z listy wraca do magazynu zamku", "to samo"],
    ["obrońców tylu, ilu trzeba", "najsilniejszy obrońca zamienia się poziomem z najsłabszym wolnym "
     "rycerzem w magazynie zamku (gdy taki słabszy jest) – w zamku zostają najsłabsi, silni idą dalej",
     "zamiana poziomów tylko między najsłabszym a ostatnim obrońcą; z magazynem nic – silni zostają w zamku"],
], [3.5, 7.2, 6.8], bad_rows=(3,)))

# ---------------------------------------------------------------- zgodne
s.append(PageBreak())
s.append(P("3. Co jest zgodne z oryginałem (wybrane liczby)", H1))
s.append(table([
    ["Co", "Zasada (oryginał = port)"],
    ["Młyn, piekarnia", "1 zboże → 1 mąka, 1 mąka → 1 chleb"],
    ["Chlewnia, rzeźnia", "zboże na cykl; rozmnażanie wg tablicy 6000…14000 (1–7 świń); 1 świnia → 1 mięso"],
    ["Huty, zbrojownia", "1 ruda + 1 węgiel → stal / sztabka; 1 węgiel + 1 stal → miecz, potem tarcza"],
    ["Narzędziownia, stocznia, tartak", "deska + stal → narzędzie wg priorytetów; deska → łódź; pień → deska"],
    ["Geolog", "8 losowań w promieniu 64 pól, tylko góry; znaczek mały poniżej 12 jednostek złoża"],
    ["Złoto i morale", "strażnice: chata 2, wieża 4, twierdza 8 sztabek (tylko z rycerzem); morale = "
     "1024 + 10240·gracze·złoto gracza / całe złoto, zamki ±1024"],
    ["Szkolenie rycerzy", "próba co 60 s; progi zamek / magazyn 4000 / 2000 / 1000 / 500, twierdza ½, "
     "wieża ¼, chata 1/16 tego; średnio 0 → 4 w magazynie 4,1 h gry"],
    ["Walka", "siła = 2<super>poziom</super> · (na cudzej ziemi morale); napastnik wygrywa z szansą "
     "siła_n / (siła_n + siła_o)"],
    ["Rycerze z osadników", "nowy osadnik co (60 − rozmnażanie)·50 tików; suwak domyślnie ≈ 31% "
     "rycerzy; potrzebny miecz i tarcza"],
    ["Transport", "kategorie dróg 4, 6, 7, 10, 13, 18, 24 pól; tragarzy na drogę 1, 2, 3, 4, 6, 8, 11, 15; "
     "≥ 7 towarów = zatkana flaga; domyślne priorytety flag"],
    ["Magazyny", "wysyłka do budynków wg 3 list (6/8, 1/8, 1/8); tryb OUT: najwyższy priorytet magazynu"],
    ["Obsada budynków", "chata 1/1/2/2/3, wieża 1/2/3/4/6, twierdza 1/3/6/9/12 rycerzy (Minimum … Pełna)"],
], [4.5, 13]))
s.append(P("Nie sprawdzono instrukcja po instrukcji: przeszukiwania sieci dróg i chodzenia "
           "tragarzy, budowania (poza zamawianiem materiałów), odsyłania rycerzy z budynków.", NOTE))

# ---------------------------------------------------------------- okna
WINDOWS = [
    ("4.1 Okno ustawień rycerzy (Knight settings)", "okno-rycerzy-opis.png", [
        ["Nr", "Nazwa", "Co robi w grze"],
        ["1, 3", "Osadnik / Rycerz (obrazki)", "końce suwaka 2"],
        ["2", "Suwak: osadnicy → rycerze", "jaka część nowych osadników zostaje rycerzami (domyślnie ≈ 31%); potrzebny miecz i tarcza"],
        ["4–7", "Szkól 1 / 5 / 20 / 100", "od razu zamienia tylu wolnych osadników w rycerzy poz. 0 (w porcie wszystkich – różnica 3)"],
        ["8", "Osadnik + tarcza + miecz", "obrazek: z czego powstaje rycerz"],
        ["9", "Gotowi do zamiany", "ilu osadników można zamienić teraz (min. z mieczy, tarcz, wolnych osadników)"],
        ["10", "Morale rycerzy %", "siła rycerzy na cudzej ziemi; zależy od złota i zamków"],
        ["11", "Złoto", "sztabki w magazynach i strażnicach"],
        ["12", "Słaby / silny rycerz", "ozdoba, bez funkcji"],
        ["13, 14", "Atak: najsłabsi / najsilniejsi", "którzy rycerze z budynków idą do ataku najpierw"],
        ["15", "Wymiana rycerzy", "budynki odsyłają rycerzy, w ich miejsce przychodzą lepiej wyszkoleni (ok. 24 s)"],
        ["16, 19", "Mniej / więcej obrońców zamku", "−1 / +1, od 1 do 99"],
        ["17, 18", "Obrońcy: chciani / są", "ustawiona liczba (domyślnie 3) / obecna"],
        ["20", "EXIT", "powrót do wyboru ustawień"],
    ]),
    ("4.2 Okno obsady budynków wojskowych (Knight level)", "okno-obsady-opis.png", [
        ["Nr", "Nazwa", "Co robi w grze"],
        ["wiersze A–D", "odległość od wroga", "A (1–7) najbliżej, B (8–14) blisko, C (15–21) daleko, D (22–28) w głębi kraju"],
        ["1, 8, 15, 22", "obrazki wierszy", "bez funkcji"],
        ["obsada (górny napis 4, 11, 18, 25; −/+ 2–3, 9–10, 16–17, 23–24)", "ilu rycerzy budynek chce",
         "Minimum / Słaba / Średnia / Dobra / Pełna: chata 1/1/2/2/3, wieża 1/2/3/4/6, twierdza 1/3/6/9/12"],
        ["rezerwa (dolny napis 7, 14, 21, 28; −/+ 5–6, 12–13, 19–20, 26–27)", "ilu zostaje przy ataku",
         "do ataku idzie tylko nadwyżka ponad rezerwę; rezerwa ≤ obsada"],
        ["29", "EXIT", "powrót do wyboru ustawień"],
    ]),
    ("4.3 Okno statystyki osadników (Serfs)", "okno-osadnikow-opis.png", [
        ["Nr", "Zawód (narzędzie)", "Nr", "Zawód (narzędzie)"],
        ["1", "Tragarz", "15", "Górnik (kilof)"],
        ["2", "Przewoźnik (łódź)", "16", "Szkutnik (młotek)"],
        ["3", "Kopacz (łopata)", "17", "Narzędziowiec (młotek + piła)"],
        ["4", "Budowniczy (młotek)", "18", "Płatnerz (młotek + obcęgi)"],
        ["5–9", "Rycerz poz. 4 … 0 (miecz + tarcza)", "19", "Rybak (wędka)"],
        ["10", "Drwal (siekiera)", "20", "Hodowca świń"],
        ["11", "Tracz (piła)", "21", "Rzeźnik (tasak)"],
        ["12", "Hutnik", "22", "Rolnik (kosa)"],
        ["13", "Kamieniarz (kilof)", "23 / 24", "Młynarz / Piekarz"],
        ["14", "Leśniczy", "25", "Geolog (młotek)"],
        ["26", "Wolny osadnik – z niego powstają wszyscy", "27 / 28", "Razem / EXIT"],
    ]),
    ("4.4 Okno trybów magazynu / zamku", "okno-magazynu-opis.png", [
        ["Nr", "Nazwa", "Co robi w grze"],
        ["1, 5", "obrazki: towary / osadnicy", "oznaczają grupy"],
        ["2, 6", "przyjmuj (IN), lewy klik", "zwykły tryb: przyjmuje i wydaje"],
        ["3, 7", "wstrzymaj (STOP), prawy klik", "nie przyjmuje nowych, trzyma, co ma"],
        ["4, 8", "wywoź / wysyłaj (OUT), prawy klik", "nie przyjmuje i wysyła do innych magazynów (opróżnianie)"],
        ["9–13", "obrońcy zamku poz. 4 … 0", "tylko w oknie zamku: ilu obrońców ma dany poziom (różnica 4)"],
        ["14 / 15", "Przewiń / EXIT", "okno towarów zamku / zamknięcie"],
    ]),
]
for i, (title, img, rows) in enumerate(WINDOWS):
    s.append(PageBreak())
    if i == 0:
        s.append(P("4. Okna gry z numerami przycisków", H1))
        s.append(P("Zrzuty z Amigi 2 (A4000 + Warp 7060), wersja 0.137. Numery na obrazach "
                   "odpowiadają tabelom – można o nie pytać („co robi 15 w oknie rycerzy?”).", NOTE))
    s.append(P(title, H2))
    s.append(shot(img))
    s.append(Spacer(1, 6))
    w = [2.8, 4.7, 10] if len(rows[0]) == 3 else [1.6, 7.1, 1.6, 7.2]
    s.append(table(rows, w))

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=1.8 * cm, rightMargin=1.8 * cm,
                        topMargin=1.6 * cm, bottomMargin=1.8 * cm,
                        title="Settlers 0.137 – raport zgodności z oryginałem",
                        author="Settlers port")
doc.build(s, onFirstPage=footer, onLaterPages=footer)
print(OUT)
