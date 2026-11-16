# Okno obsady budynków wojskowych (Knight level)

Obraz z opisem: [`docs/shots/okno-obsady-opis.png`](shots/okno-obsady-opis.png) (zrzut z Amigi 2,
16.11.2026, wersja 0.137; obraz robi `tools/annotate_knight_level_box.py`). Kod:
`freeserf/src/popup.cc` – `draw_knight_level_box`, `handle_knight_level_click`;
`building.cc` – `update_military`, `update_military_flag_state`; `player.cc` –
`change_knight_occupation`, `available_knights_at_pos`. Numery jak na obrazie.

## Wiersze A–D: odległość budynku od wroga

Każda chata, wieża i twierdza co chwilę sprawdza, czy w pobliżu leży ziemia innego gracza
(`update_military_flag_state`, pola spirali wokół budynku), i dostaje jeden z czterech stopni:

| Wiersz | Stopień | Kiedy |
|---|---|---|
| A (1–7) | 3 – najbliżej wroga | cudza ziemia w najbliższym pierścieniu sprawdzanych pól |
| B (8–14) | 2 – blisko | cudza ziemia dopiero w następnym pierścieniu |
| C (15–21) | 1 – daleko | cudza ziemia tylko w najdalszym pierścieniu |
| D (22–28) | 0 – w głębi kraju | w sprawdzanych polach brak cudzej ziemi |

Obrazki 1, 8, 15, 22 tylko pokazują wiersz (kliknięcie nic nie robi).

## Obsada i rezerwa

Każdy wiersz ma dwa ustawienia, każde na jednym z 5 poziomów: **Minimum, Słaba, Średnia, Dobra,
Pełna** (Minimum, Weak, Medium, Good, Full).

- **Obsada** (górny napis: 4, 11, 18, 25; przyciski − / +: 2–3, 9–10, 16–17, 23–24) – ilu
  rycerzy budynek **chce mieć**. Gdy ma mniej, wzywa rycerzy z magazynu; gdy więcej – odsyła
  najsłabszego.
- **Rezerwa** (dolny napis: 7, 14, 21, 28; przyciski − / +: 5–6, 12–13, 19–20, 26–27) – ilu
  rycerzy **musi zostać** w budynku przy ataku; do ataku można wysłać tylko nadwyżkę ponad
  rezerwę. Rezerwa nie może być wyższa niż obsada (i obsada niższa niż rezerwa).

Liczba rycerzy dla poziomu (te same tablice w oryginale Amigi: 0xBB5E, 0xBB9C, 0xBBDA):

| Poziom | Chata | Wieża | Twierdza |
|---|---|---|---|
| Minimum | 1 | 1 | 1 |
| Słaba | 1 | 2 | 3 |
| Średnia | 2 | 3 | 6 |
| Dobra | 2 | 4 | 9 |
| Pełna | 3 | 6 | 12 |

Podczas **wymiany rycerzy** (okno rycerzy, przycisk 15) obsada przez chwilę spada do niższej
tablicy (chata 1, 1, 1, 1, 2; wieża 1, 1, 2, 3, 4; twierdza 1, 2, 4, 6, 8) – budynki odsyłają
rycerzy, potem przychodzą lepsi.

Domyślnie (jak na zrzucie): A – Pełna / Dobra, B – Dobra / Średnia, C – Średnia / Słaba,
D – Słaba / Minimum.

29 – EXIT: powrót do okna wyboru ustawień.

Przykład: twierdza w wierszu A z obsadą Pełna i rezerwą Dobra chce 12 rycerzy; do ataku może
wysłać najwyżej 12 − 9 = 3.
