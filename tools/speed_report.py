#!/usr/bin/env python3
"""The speed-up report (PDF, Polish and English): Settlers (Freeserf) on the
Amiga, 0.125 -> 0.129.

usage: python tools/speed_report.py

Data: docs/speed/*.csv and the numbers below (measured on Amiga 1, see
docs/PORTING.md). The picture of the scene and the PDFs go to
private/speed-report/ (the picture shows the original game's graphics: not
in the repository).
"""
import os

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.pdfmetrics import registerFontFamily
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, PageBreak, Paragraph,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'private', 'speed-report')
SHOT = os.path.join(OUT, 'scena-960x540.png')

FONTS = r'C:\Windows\Fonts'
pdfmetrics.registerFont(TTFont('Arial', os.path.join(FONTS, 'arial.ttf')))
pdfmetrics.registerFont(TTFont('Arial-Bold', os.path.join(FONTS, 'arialbd.ttf')))
pdfmetrics.registerFont(TTFont('Arial-Italic', os.path.join(FONTS, 'ariali.ttf')))
registerFontFamily('Arial', normal='Arial', bold='Arial-Bold',
                   italic='Arial-Italic', boldItalic='Arial-Bold')
plt.rcParams['font.family'] = 'Arial'

INK = '#1f2933'
C125, C126, C128, C129 = '#b0b7c0', '#7f8c99', '#3d7ab8', '#e07b24'

# --- measurements (Amiga 1, save 20-last, Full speed, 3 readings each) ---
MODES = ['640×360', '800×600', '960×540']
FPS = {
    '0.125': [4.5, 4.1, 4.0],
    '0.126': [18.8, 10.1, 9.6],
    '0.128': [30.3, 15.9, 14.1],
    '0.129': [37.9, 20.1, 18.2],
}
STEPS = {
    '0.125': [22, 20, 20],
    '0.126': [50, 49, 48],
    '0.128': [50, 50, 50],
    '0.129': [50, 50, 50],
}
LOGIC_DRAW = {    # 960x540: ms of each second on logic, on drawing
    '0.125': (809, 182),
    '0.126': (558, 426),
    '0.128': (397, 580),
    '0.129': (395, 574),
}
STEP_MS = [
    ('0.125', 35.3), ('0.125 +5', 28.9), ('+6', 27.0), ('+7', 23.9),
    ('+8', 16.0), ('+10', 13.9), ('0.126', 10.9), ('0.127', 10.8),
    ('0.128 a', 9.36), ('0.128 b', 8.18), ('0.128 c', 7.58),
    ('0.128', 7.52),
]

LANG = 'pl'


def T(pl, en):
    return pl if LANG == 'pl' else en


def num(v, d=1):
    s = ('%.' + str(d) + 'f') % v
    return s.replace('.', ',') if LANG == 'pl' else s


styles = getSampleStyleSheet()
H1 = ParagraphStyle('H1', parent=styles['Heading1'], fontName='Arial-Bold',
                    fontSize=16, textColor=INK, spaceAfter=6)
H2 = ParagraphStyle('H2', parent=styles['Heading2'], fontName='Arial-Bold',
                    fontSize=12.5, textColor=INK, spaceBefore=8, spaceAfter=4)
BODY = ParagraphStyle('B', parent=styles['BodyText'], fontName='Arial',
                      fontSize=9.5, leading=13, textColor=INK)
SMALL = ParagraphStyle('S', parent=BODY, fontSize=8, leading=10.5,
                       textColor='#52606d')
TITLE = ParagraphStyle('T', parent=H1, fontSize=22, leading=27,
                       alignment=TA_CENTER, spaceAfter=4)
SUB = ParagraphStyle('Sub', parent=BODY, fontSize=11, alignment=TA_CENTER,
                     textColor='#52606d')
CELL = ParagraphStyle('C', parent=BODY, fontSize=8.5, leading=10.5)


def table(rows, widths, head=1, align_right_from=1):
    data = [[Paragraph(str(c), CELL) if isinstance(c, str) and len(c) > 18
             else c for c in r] for r in rows]
    t = Table(data, colWidths=widths, repeatRows=head)
    t.setStyle(TableStyle([
        ('FONT', (0, 0), (-1, -1), 'Arial', 8.5),
        ('FONT', (0, 0), (-1, head - 1), 'Arial-Bold', 8.5),
        ('TEXTCOLOR', (0, 0), (-1, -1), INK),
        ('LINEBELOW', (0, head - 1), (-1, head - 1), 0.8, INK),
        ('LINEBELOW', (0, head), (-1, -1), 0.25, '#d5dae0'),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('ALIGN', (align_right_from, 0), (-1, -1), 'RIGHT'),
        ('TOPPADDING', (0, 0), (-1, -1), 2.5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 2.5),
    ]))
    return t


def chart_fps(path):
    fig, ax = plt.subplots(figsize=(7.2, 3.3), dpi=200)
    cols = [C125, C126, C128, C129]
    w = 0.2
    for i, v in enumerate(FPS):
        xs = [m + (i - 1.5) * w for m in range(len(MODES))]
        ax.bar(xs, FPS[v], w, color=cols[i], label=v)
        for x, y in zip(xs, FPS[v]):
            ax.text(x, y + 0.6, num(y), ha='center', va='bottom', fontsize=7,
                    color=INK)
    ax.set_xticks(range(len(MODES)))
    ax.set_xticklabels(MODES)
    ax.set_ylabel(T('klatki na sekundę', 'frames per second'))
    ax.set_ylim(0, 43)
    ax.spines[['top', 'right']].set_visible(False)
    ax.legend(frameon=False, ncol=4, loc='upper right', fontsize=8)
    ax.set_title(T('Klatki na sekundę (zapis 20-last, Full speed, Amiga 1)',
                   'Frames per second (save 20-last, Full speed, Amiga 1)'),
                 fontsize=10, color=INK)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def chart_time(path):
    fig, ax = plt.subplots(figsize=(7.2, 2.8), dpi=200)
    vers = list(LOGIC_DRAW)
    lg = [LOGIC_DRAW[v][0] for v in vers]
    dr = [LOGIC_DRAW[v][1] for v in vers]
    ax.barh(vers, lg, color='#3d7ab8', label=T('logika gry', 'game logic'))
    ax.barh(vers, dr, left=lg, color='#e07b24',
            label=T('rysowanie', 'drawing'))
    for i, v in enumerate(vers):
        ax.text(lg[i] / 2, i, '%d' % lg[i], ha='center', va='center',
                color='white', fontsize=8)
        ax.text(lg[i] + dr[i] / 2, i, '%d' % dr[i], ha='center',
                va='center', color='white', fontsize=8)
        ax.text(1005, i, T('%s kl./s, %d kroków/s', '%s fps, %d steps/s') % (
            num(FPS[v][2]), STEPS[v][2]), va='center', fontsize=8, color=INK)
    ax.set_xlim(0, 1300)
    ax.invert_yaxis()
    ax.set_xlabel(T('milisekundy z każdej sekundy (960×540)',
                    'milliseconds of each second (960×540)'))
    ax.spines[['top', 'right']].set_visible(False)
    ax.legend(frameon=False, ncol=2, loc='lower center',
              bbox_to_anchor=(0.42, 1.0), fontsize=8)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def chart_steps(path):
    fig, ax = plt.subplots(figsize=(7.2, 2.9), dpi=200)
    names = [n for n, _ in STEP_MS]
    vals = [v for _, v in STEP_MS]
    cols = [C129 if n == '0.128' else '#3d7ab8' for n in names]
    ax.bar(range(len(vals)), vals, color=cols)
    for i, v in enumerate(vals):
        ax.text(i, v + 0.5, num(v), ha='center', fontsize=7, color=INK)
    ax.axhline(20, color='#c0392b', lw=0.8, ls='--')
    ax.text(len(vals) - 0.5, 20.6,
            T('20 ms = granica pełnego tempa (50 kroków/s)',
              '20 ms = limit of the full game speed (50 steps/s)'),
            ha='right', fontsize=7, color='#c0392b')
    ax.set_xticks(range(len(vals)))
    ax.set_xticklabels(names, fontsize=7, rotation=30, ha='right')
    ax.set_ylabel(T('ms na krok', 'ms per step'))
    ax.set_ylim(0, 40)
    ax.spines[['top', 'right']].set_visible(False)
    ax.set_title(T('Jeden krok logiki gry (simtest, 1000 kroków zapisu 20-last)',
                   'One step of the game logic (simtest, 1000 steps of save '
                   '20-last)'), fontsize=10, color=INK)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def on_page(canvas, doc):
    canvas.saveState()
    canvas.setFont('Arial', 7.5)
    canvas.setFillColor(colors.HexColor('#7b8794'))
    canvas.drawString(18 * mm, 10 * mm,
                      T('Settlers (Freeserf) na Amigę – przyspieszenie '
                        '0.125 → 0.129',
                        'Settlers (Freeserf) on the Amiga – speed-up '
                        '0.125 → 0.129'))
    canvas.drawRightString(A4[0] - 18 * mm, 10 * mm,
                           T('strona %d', 'page %d') % doc.page)
    canvas.restoreState()


def build(lang):
    global LANG
    LANG = lang
    os.makedirs(OUT, exist_ok=True)
    f1 = os.path.join(OUT, 'chart-fps-%s.png' % lang)
    f2 = os.path.join(OUT, 'chart-time-%s.png' % lang)
    f3 = os.path.join(OUT, 'chart-step-%s.png' % lang)
    chart_fps(f1)
    chart_time(f2)
    chart_steps(f3)
    pdf = os.path.join(OUT, T('Settlers-przyspieszenie-0.125-0.129.pdf',
                              'Settlers-speed-up-0.125-0.129.pdf'))
    doc = SimpleDocTemplate(
        pdf, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
        topMargin=16 * mm, bottomMargin=16 * mm,
        title=T('Settlers na Amigę – przyspieszenie 0.125–0.129',
                'Settlers on the Amiga – speed-up 0.125–0.129'),
        author='Settlers Amiga port')
    W = A4[0] - 36 * mm
    s = []
    s.append(Spacer(1, 8 * mm))
    s.append(Paragraph(T('Settlers (Freeserf) na Amigę',
                         'Settlers (Freeserf) on the Amiga'), TITLE))
    s.append(Paragraph(T('Przyspieszenie dużej rozgrywki: wersje 0.125 → 0.129',
                         'Speeding up a large game: versions 0.125 → 0.129'),
                       SUB))
    s.append(Paragraph(T('7 listopada 2026', '7 November 2026') +
                       ' · Amiga 1200 + Warp 1260 (MC68060), AmigaOS 3.2, ' +
                       T('karta graficzna Warpa (RTG)',
                         'the Warp\'s graphics (RTG)'), SUB))
    s.append(Spacer(1, 7 * mm))

    s.append(Paragraph(T('Najważniejsze', 'Summary'), H2))
    s.append(Paragraph(T(
        'Na tej samej, najcięższej scenie (zapis <b>20-last</b>, 2521 postaci) '
        'gra w wersji 0.125 dawała <b>4–4,5 klatki na sekundę</b>, a czas w '
        'grze płynął 2,5 raza za wolno (20–22 zamiast 50 kroków logiki na '
        'sekundę). Wersja 0.129 utrzymuje <b>pełne tempo gry</b> (50 kroków '
        'na sekundę) i pokazuje <b>37,9 kl./s w 640×360, 20,1 kl./s w '
        '800×600 i 18,2 kl./s w 960×540</b> – od 4,6 do 8,4 raza więcej. '
        'Jeden krok logiki skrócił się z 35,3 do 7,5 ms (4,7×). Przebieg gry '
        'się nie zmienił: zapis po 1000 krokach i gra komputerowa 30 000 '
        'kroków są identyczne ze starą wersją, obraz identyczny co do piksela.',
        'On the same, heaviest scene (save <b>20-last</b>, 2521 serfs) '
        'version 0.125 gave <b>4–4.5 frames per second</b>, and game time ran '
        '2.5 times too slow (20–22 instead of 50 logic steps a second). '
        'Version 0.129 keeps <b>the full game speed</b> (50 steps a second) '
        'and shows <b>37.9 fps at 640×360, 20.1 fps at 800×600 and 18.2 fps '
        'at 960×540</b> – 4.6 to 8.4 times more. One logic step went from '
        '35.3 to 7.5 ms (4.7×). The game itself is unchanged: the save after '
        '1000 steps and a 30,000-step computer game are identical to the old '
        'version, and the picture is identical pixel for pixel.'), BODY))
    s.append(Spacer(1, 4 * mm))
    s.append(Image(f1, width=W, height=W * 3.3 / 7.2))
    s.append(Spacer(1, 3 * mm))
    rows = [[T('Rozdzielczość', 'Resolution'), '0.125', '0.126', '0.128',
             '0.129', '0.129 / 0.125']]
    for i, m in enumerate(MODES):
        rows.append([m] + [num(FPS[v][i]) for v in FPS] +
                    ['×' + num(FPS['0.129'][i] / FPS['0.125'][i])])
    s.append(table(rows, [W * 0.24] + [W * 0.15] * 4 + [W * 0.16]))
    s.append(Spacer(1, 2 * mm))
    rows = [[T('Kroki logiki na sekundę', 'Logic steps per second'),
             '0.125', '0.126', '0.128', '0.129']]
    for i, m in enumerate(MODES):
        rows.append([m] + [str(STEPS[v][i]) for v in STEPS])
    s.append(table(rows, [W * 0.32] + [W * 0.17] * 4))
    s.append(Paragraph(T(
        'Kroki logiki: 50 na sekundę to pełne tempo gry (oryginał). Mniej – '
        'czas w grze płynie wolniej.',
        'Logic steps: 50 a second is the full game speed (the original). '
        'Fewer – game time runs slower.'), SMALL))
    s.append(PageBreak())

    # the scene
    s.append(Paragraph(T('Scena testowa', 'The test scene'), H1))
    s.append(Paragraph(T(
        'Wszystkie pomiary na jednym zapisie gry, <b>20-last.save</b> – '
        'rozgrywka zaawansowana, z którą użytkownik zgłosił „5 fps”. Ta sama '
        'scena, ten sam widok (miejsce mapy zapisane w grze), dla każdej '
        'wersji i rozdzielczości.',
        'All measurements use one saved game, <b>20-last.save</b> – an '
        'advanced game the user reported as running at “5 fps”. The same '
        'scene and the same view (the map position stored in the save) for '
        'every version and resolution.'), BODY))
    s.append(Spacer(1, 3 * mm))
    if os.path.exists(SHOT):
        s.append(Image(SHOT, width=W, height=W * 540 / 960))
        s.append(Paragraph(T('Scena w 960×540 (wersja 0.129, gra zatrzymana).',
                             'The scene at 960×540 (version 0.129, game '
                             'paused).'), SMALL))
    s.append(Spacer(1, 3 * mm))
    rows = [
        [T('Cecha', 'Item'), T('Wartość', 'Value')],
        [T('Mapa', 'Map'), T('64 × 64 pola (4096), misja nr 20 '
                             '(mission_level 19)',
                             '64 × 64 places (4096), mission 20 '
                             '(mission_level 19)')],
        [T('Czas gry', 'Game time'), T('1 866 067 ticków', '1,866,067 ticks')],
        [T('Gracze', 'Players'),
         T('3: gracz (turkusowy) i dwóch graczy komputerowych (czerwony, '
           'fioletowy)', '3: the player (cyan) and two computer players '
           '(red, purple)')],
        [T('Postacie', 'Serfs'),
         T('2521 (po 1000 krokach 2524): 1573 bez zawodu w magazynach, 542 '
           'tragarzy, 177 rycerzy, 46 geologów, reszta rzemieślnicy',
           '2521 (2524 after 1000 steps): 1573 generic serfs in stocks, 542 '
           'transporters, 177 knights, 46 geologists, the rest craftsmen')],
        [T('Postacie czekające w magazynach', 'Serfs waiting in stocks'),
         T('1787 (pozostałe ~737 pracuje w każdym kroku)',
           '1787 (the other ~737 work in every step)')],
        [T('Flagi (węzły dróg)', 'Flags (road nodes)'), '371'],
        [T('Budynki', 'Buildings'),
         T('193 (w tym 3 zamki, 4 magazyny, 56 wież strażniczych, 43 '
           'kopalnie, 19 farm, 12 młynów)',
           '193 (3 castles, 4 stocks, 56 huts, 43 mines, 19 farms, 12 '
           'mills among them)')],
        [T('Pola z drogą', 'Places with a road'), '1474'],
        [T('Drzewa', 'Trees'), '274'],
        [T('Kamienie', 'Stones'), '25'],
        [T('Inne obiekty mapy', 'Other map objects'),
         T('~465 (pola uprawne, sadzonki, pnie, kaktusy, znaki geologów)',
           '~465 (fields, seedlings, stumps, cacti, geologists\' signs)')],
    ]
    s.append(table(rows, [W * 0.32, W * 0.68], align_right_from=9))
    s.append(Spacer(1, 3 * mm))
    rows = [[T('Gracz', 'Player'), T('Budynki', 'Buildings'),
             T('Flagi', 'Flags'), T('Postacie', 'Serfs')],
            [T('gracz 0 (człowiek)', 'player 0 (human)'), '64', '84', '727'],
            [T('gracz 1 (komputer)', 'player 1 (computer)'), '58', '133',
             '803'],
            [T('gracz 2 (komputer)', 'player 2 (computer)'), '71', '154',
             '991']]
    s.append(table(rows, [W * 0.4, W * 0.2, W * 0.2, W * 0.2]))
    s.append(Spacer(1, 3 * mm))
    s.append(Paragraph(T(
        'Na ekranie w jednej klatce: ok. 370 rysunków w 640×360 (sprite\'y '
        'drzew, budynków i postaci z cieniami oraz kawałki terenu), ok. 740 '
        'w 800×600 i ok. 860 w 960×540. Logika liczy w każdym kroku całą '
        'mapę – wszystkie postacie, flagi i budynki, nie tylko widoczne.',
        'On screen in one frame: about 370 drawings at 640×360 (sprites of '
        'trees, buildings and serfs with their shadows, and pieces of the '
        'landscape), about 740 at 800×600 and about 860 at 960×540. The logic '
        'works on the whole map in every step – all serfs, flags and '
        'buildings, not only the visible ones.'), BODY))
    s.append(PageBreak())

    # method
    s.append(Paragraph(T('Jak mierzono', 'How it was measured'), H1))
    for t in [
        T('<b>Maszyna:</b> Amiga 1 – A1200 z kartą Warp 1260 (MC68060, '
          'pamięć Fast RAM i grafika na karcie), AmigaOS 3.2.2.1. Wszystkie '
          'wyniki z tej jednej maszyny.',
          '<b>Machine:</b> Amiga 1 – an A1200 with a Warp 1260 card '
          '(MC68060, Fast RAM and graphics on the card), AmigaOS 3.2.2.1. All '
          'results come from this one machine.'),
        T('<b>W grze:</b> opcja STATS (linia w RAM:Settlers-stats.txt co '
          'sekundę) – klatki na sekundę, ms z każdej sekundy na logikę i na '
          'rysowanie, kroki logiki na sekundę; po trzy odczyty, co 5 s, po '
          'wczytaniu zapisu. Ustawienie Large games: Full speed (0.125 nie ma '
          'tej opcji – zawsze nadrabia do 5 kroków na klatkę, czyli tak samo). '
          'Rozdzielczość podana w wierszu poleceń (MODE=).',
          '<b>In the game:</b> the STATS option (a line in '
          'RAM:Settlers-stats.txt every second) – frames per second, ms of '
          'each second on logic and on drawing, logic steps per second; three '
          'readings 5 s apart after the save is loaded. Large games setting: '
          'Full speed (0.125 has no such option – it always catches up with '
          'up to 5 steps a frame, the same thing). The resolution is given on '
          'the command line (MODE=).'),
        T('<b>Logika osobno:</b> program simtest (ten sam kod gry bez obrazu) '
          '– 1000 kroków zapisu 20-last; czas kroku i porównanie zapisu po '
          '1000 krokach ze starą wersją (identyczny), a także gra 4 graczy '
          'komputerowych 30 000 kroków (identyczna).',
          '<b>Logic alone:</b> the simtest program (the same game code, no '
          'picture) – 1000 steps of save 20-last; the step time, and the save '
          'after 1000 steps compared with the old version (identical), also a '
          '30,000-step game of 4 computer players (identical).'),
        T('<b>Obraz:</b> zrzut ekranu przy zatrzymanej grze (FREEZE, F9) '
          'porównany piksel po pikselu ze starą wersją – identyczny (poza '
          'licznikiem FPS).',
          '<b>Picture:</b> a screenshot of the paused game (FREEZE, F9) '
          'compared pixel by pixel with the old version – identical (except '
          'the FPS counter).'),
        T('<b>Gdzie idzie czas:</b> profiler próbkujący (nowy, PCSAMPLE): co '
          '4 ms zapisuje, w której funkcji jest gra; tysiące próbek dają '
          'udział każdej funkcji.',
          '<b>Where the time goes:</b> a sampling profiler (new, PCSAMPLE): '
          'every 4 ms it notes which function the game is in; thousands of '
          'samples give the share of each function.'),
    ]:
        s.append(Paragraph(t, BODY))
        s.append(Spacer(1, 1.5 * mm))

    s.append(Paragraph(T('Na co idzie czas (960×540)',
                         'Where the time goes (960×540)'), H2))
    s.append(Image(f2, width=W, height=W * 2.8 / 7.2))
    s.append(Paragraph(T(
        'W 0.125 logika zajmowała 810 ms z każdej sekundy – na obraz '
        'zostawało 0,18 s, a i tak gra nie nadążała z krokami. Od 0.128 '
        'logika potrzebuje ok. 400 ms na pełne 50 kroków, reszta idzie na '
        'obraz; w 0.129 ten sam czas rysowania daje więcej klatek.',
        'In 0.125 the logic took 810 ms of each second – 0.18 s was left for '
        'the picture, and the game still could not keep up with its steps. '
        'From 0.128 on the logic needs about 400 ms for the full 50 steps, '
        'the rest goes to the picture; in 0.129 the same drawing time gives '
        'more frames.'), BODY))
    s.append(PageBreak())

    # logic
    s.append(Paragraph(T('Logika gry', 'Game logic'), H1))
    s.append(Image(f3, width=W, height=W * 2.9 / 7.2))
    rows = [
        [T('Zmiana', 'Change'), T('Krok (ms)', 'Step (ms)'),
         T('Na czym polega', 'What it is')],
        [T('start (0.125)', 'start (0.125)'), num(35.3),
         T('w każdym kroku budynki i flagi przeszukiwały całą sieć dróg, choć '
           'żaden magazyn nie miał potrzebnej postaci',
           'in every step buildings and flags searched the whole road network '
           'although no stock had the serf they needed')],
        [T('test magazynów', 'stock test'), num(28.9),
         T('przed przeszukaniem sprawdzenie, czy jakikolwiek magazyn gracza '
           'może wysłać postać',
           'before a search, a check whether any stock of the player can send '
           'the serf')],
        [T('kolejka szukania', 'search queue'), num(27.0),
         T('kolejka przeszukania bez przesuwania tablicy przy każdym zdjęciu',
           'the search queue without moving the array on every take')],
        [T('tablice zamiast map', 'arrays, not maps'), num(23.9),
         T('postacie magazynu w tablicy, listy i kolejki trzymane między '
           'krokami',
           'the serfs of a stock in an array, lists and queues kept between '
           'steps')],
        [T('części sieci dróg', 'road network parts'), num(16.0),
         T('test magazynów tylko w tej części sieci dróg, do której należy '
           'flaga',
           'the stock test only in the part of the road network the flag '
           'belongs to')],
        [T('kierunki bez dzielenia', 'directions, no division'), num(13.9),
         T('obieg 6 kierunków bez dzielenia (68060 liczy je długo)',
           'the cycle of 6 directions without a division (slow on a 68060)')],
        ['0.126', num(10.9),
         T('postać czytana raz, mniejsza flaga, stałe tablice',
           'a serf read once, a smaller flag, constant tables')],
        [T('tablica postaci', 'serf table'), num(9.36, 2),
         T('stan, typ i magazyn każdej postaci w zwartej tablicy',
           'the state, type and stock of every serf in a compact table')],
        [T('pętla po tablicy', 'loop over the table'), num(8.18, 2),
         T('pętla postaci bez listy indeksów robionej co krok',
           'the serf loop without a list of indexes made every step')],
        ['0.128', num(7.52, 2),
         T('bez zbędnego odświeżania tablicy, zmienne lokalne',
           'no needless table update, locals')],
    ]
    s.append(table(rows, [W * 0.22, W * 0.12, W * 0.66], align_right_from=1))
    s.append(Paragraph(T(
        'Teraz krok to w większości czekanie procesora na pamięć przy każdej '
        'fladze i każdej pracującej postaci (68060 ma 8 KB pamięci '
        'podręcznej danych, a scena – tysiące obiektów).',
        'Now a step is mostly the CPU waiting for memory at every flag and '
        'every working serf (the 68060 has an 8 KB data cache, the scene has '
        'thousands of objects).'), BODY))

    s.append(Paragraph(T('Rysowanie', 'Drawing'), H1))
    rows = [
        [T('Zmiana (0.128 → 0.129)', 'Change (0.128 → 0.129)'), '640×360',
         '960×540'],
        ['0.128', num(30.3), num(14.1)],
        [T('kopiowanie pikseli sprite\'a bez pętli po bajtach',
           'sprite pixels copied without a loop by bytes'), num(32.2),
         num(14.5)],
        [T('tablica obrazków: lepszy skrót (drzewo i jego cień już się nie '
           'wypychają)',
           'image table: a better hash (a tree and its shadow no longer push '
           'each other out)'), num(32.4), num(15.4)],
        [T('krótsze dekodowanie, dziki bez dzielenia',
           'shorter run decoding, wild boars without a division'), num(33.7),
         num(16.1)],
        [T('rysowanie sprite\'ów w asemblerze 68k',
           'sprites drawn in 68k assembly'), num(35.7), num(17.1)],
        [T('krótka ścieżka dla zwykłego sprite\'a',
           'a short path for an ordinary sprite'), num(37.9), num(18.2)],
    ]
    s.append(table(rows, [W * 0.64, W * 0.18, W * 0.18]))
    s.append(Spacer(1, 2 * mm))
    s.append(Paragraph(T(
        'Sprawdzone i odrzucone: rysowanie tylko zmienionych bloków ekranu '
        '(zmienia się 13–20% bloków, ale są rozsiane po całym widoku – '
        'przerysowanie kosztowało tyle co całość). Najwięcej czasu zostaje '
        'teraz w zapisie pikseli do pamięci karty i w odczycie jej pod '
        'cieniami – to granica sprzętu.',
        'Tried and dropped: drawing only the changed blocks of the screen '
        '(13–20% of the blocks change, but they are spread over the whole '
        'view – drawing them again cost as much as the whole). Most of the '
        'time now goes to writing pixels into the card\'s memory and reading '
        'it under the shadows – the limit of the hardware.'), BODY))

    s.append(Paragraph(T('Gdzie idzie czas teraz (profil, 960×540, 0.129)',
                         'Where the time goes now (profile, 960×540, 0.129)'),
                       H2))
    rows = [[T('Część', 'Part'), T('Udział', 'Share'),
             T('Co robi', 'What it does')],
            [T('rysowanie sprite\'ów (asembler)', 'drawing sprites (assembly)'),
             '24%', T('piksele drzew, budynków, postaci, cieni',
                      'the pixels of trees, buildings, serfs, shadows')],
            [T('aktualizacja postaci (pętla + stany)',
               'serf update (loop + states)'), '~16%',
             T('2524 postacie co krok', '2524 serfs every step')],
            [T('przygotowanie sprite\'a (draw_image, draw_sprite)',
               'preparing a sprite (draw_image, draw_sprite)'), '~13%',
             T('wybór obrazka, przycięcie', 'picking the image, clipping')],
            [T('flagi', 'flags'), '~11%',
             T('towary na flagach, wzywanie tragarzy',
               'goods on flags, calling transporters')],
            [T('kod widoku', 'view code'), '~7%',
             T('przejście po polach mapy na ekranie',
               'going through the map places on screen')],
            [T('budynki i reszta logiki', 'buildings and the rest of the '
               'logic'), '~10%', ''],
            ]
    s.append(table(rows, [W * 0.42, W * 0.12, W * 0.46], align_right_from=1))
    # picture check 0.125 vs 0.129
    s.append(PageBreak())
    s.append(Paragraph(T('Poprawność obrazu: 0.125 ↔ 0.129',
                         'Picture correctness: 0.125 ↔ 0.129'), H1))
    s.append(Paragraph(T(
        'Czy przyspieszenie nie zmieniło tego, co gra rysuje? Dla 4 zapisów '
        'gry i 5 rozdzielczości zrobiono zrzut ekranu w wersji 0.125 i 0.129 '
        'przy zatrzymanej grze (FREEZE, F9) i porównano je piksel po pikselu '
        '(bez licznika FPS w rogu) – 20 par, ok. 40 minut na Amidze 1.',
        'Did the speed-up change what the game draws? For 4 saved games and '
        '5 resolutions a screenshot of version 0.125 and of 0.129 was taken '
        'with the game paused (FREEZE, F9) and compared pixel by pixel '
        '(without the FPS counter in the corner) – 20 pairs, about 40 '
        'minutes on Amiga 1.'), BODY))
    s.append(Spacer(1, 3 * mm))
    ok = T('identyczne', 'identical')
    ic = T('ikonka *', 'icon *')
    res = {
        '20-last': [ok, ok, ic, ic, ok],
        'terra': [ok, ok, ok, ok, ok],
        'Misja1': [ic, ic, ic, ok, ok],
        'water-line': [ok, ok, ok, ok, ok],
    }
    cmodes = ['640×360', '640×480', '640×512', '800×600', '960×540']
    rows = [[T('Zapis', 'Save')] + cmodes]
    for k, v in res.items():
        rows.append([k] + v)
    t = table(rows, [W * 0.2] + [W * 0.16] * 5, align_right_from=9)
    for r, (k, v) in enumerate(res.items(), start=1):
        for c, x in enumerate(v, start=1):
            t.setStyle(TableStyle([('BACKGROUND', (c, r), (c, r),
                                    '#dff3e4' if x == ok else '#fdf0d5')]))
    s.append(t)
    s.append(Spacer(1, 3 * mm))
    s.append(Paragraph(T(
        '<b>Wynik:</b> 15 par identycznych co do piksela. W 5 parach różni '
        'się po 90 pikseli – zawsze ta sama <b>migająca ikonka wiadomości na '
        'panelu</b> (*): raz widoczna w 0.125, raz w 0.129, zależnie od '
        'chwili zrzutu. Widok mapy – teren, drogi, drzewa, budynki, postacie, '
        'cienie, woda i fale (zapis water-line) – jest we wszystkich parach '
        'identyczny.',
        '<b>Result:</b> 15 pairs identical pixel for pixel. 5 pairs differ in '
        '90 pixels – always the same <b>blinking message icon on the '
        'panel</b> (*): shown once in 0.125, once in 0.129, depending on the '
        'moment of the screenshot. The map view – landscape, roads, trees, '
        'buildings, serfs, shadows, water and waves (save water-line) – is '
        'identical in all pairs.'), BODY))
    icon = os.path.join(OUT, 'icon-diff.png')
    if os.path.exists(icon):
        s.append(Spacer(1, 3 * mm))
        s.append(Image(icon, width=W * 0.6, height=W * 0.6 * 150 / 500))
        s.append(Paragraph(T(
            '* Misja1, 640×360: ten sam fragment panelu w 0.125 (z lewej, bez '
            'ikonki) i w 0.129 (z prawej, ikonka wiadomości).',
            '* Misja1, 640×360: the same piece of the panel in 0.125 (left, '
            'no icon) and in 0.129 (right, the message icon).'), SMALL))
    s.append(Spacer(1, 3 * mm))
    s.append(Paragraph(T(
        'Zapisy: 20-last (duża gra, 2521 postaci), terra (inna gra '
        'użytkownika), Misja1 (pierwsza misja), water-line (wybrzeże z wodą). '
        'W 0.125 pauza nie zatrzymuje logiki (pełna pauza od 0.127) – mimo to '
        'obraz zgodny. Wcześniej każda zmiana rysowania 0.128 → 0.129 była '
        'sprawdzana tak samo (zapis 20-last, 960×540).',
        'Saves: 20-last (a large game, 2521 serfs), terra (another game of '
        'the user), Misja1 (the first mission), water-line (a coast '
        'with water). In 0.125 the pause does not stop the logic (a full '
        'pause from 0.127 on) – the picture matches all the same. Before '
        'that each drawing change 0.128 → 0.129 was checked the same way '
        '(save 20-last, 960×540).'), BODY))
    s.append(Spacer(1, 4 * mm))
    s.append(Paragraph(T(
        'Wersje do porównania na Amidze 1 (WorkSD:Games/Settlers): '
        'Settlers-0.125, Settlers-0.126, Settlers-0.128, Settlers-0.129. '
        'Szczegóły każdej zmiany: docs/PORTING.md; dane: docs/speed/*.csv.',
        'Versions for comparison on Amiga 1 (WorkSD:Games/Settlers): '
        'Settlers-0.125, Settlers-0.126, Settlers-0.128, Settlers-0.129. '
        'Details of each change: docs/PORTING.md; data: docs/speed/*.csv.'),
        SMALL))
    doc.build(s, onFirstPage=on_page, onLaterPages=on_page)
    print(pdf)


if __name__ == '__main__':
    build('pl')
    build('en')
