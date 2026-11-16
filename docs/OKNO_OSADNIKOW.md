# Okno statystyki osadników (Serfs)

Obraz z opisem: [`docs/shots/okno-osadnikow-opis.png`](shots/okno-osadnikow-opis.png) (zrzut z
Amigi 2, 16.11.2026, wersja 0.137; obraz robi `tools/annotate_serfs_box.py`). Kod:
`freeserf/src/popup.cc` – `draw_stat_6_box`, `draw_serfs_box`. Numery jak na obrazie.

Okno tylko pokazuje liczby (poza EXIT nic się w nim nie klika). Liczba przy obrazku = ilu
osadników tego zawodu ma gracz: pracujących w budynkach, idących i czekających w magazynach.
Ten sam układ ma okno osadników jednego magazynu / zamku (tam z przyciskiem przewijania).

Zawód powstaje z wolnego osadnika (26) w magazynie, gdy budynek go wezwie; magazyn zużywa wtedy
narzędzie z kolumny „Potrzebuje” (bez narzędzia osadnik nie dostanie zawodu – budynek czeka).

| Nr | Zawód | Potrzebuje | Co robi |
|---|---|---|---|
| 1 | Tragarz | – | nosi towary po drogach (jeden na odcinek drogi, więcej przy długich i zatłoczonych) |
| 2 | Przewoźnik | łódź | tragarz na drodze przez wodę |
| 3 | Kopacz | łopata | wyrównuje teren pod duże budynki |
| 4 | Budowniczy | młotek | stawia budynki |
| 5–9 | Rycerz poz. 4 … 0 | miecz + tarcza (poz. 0) | obsada budynków wojskowych, atak i obrona; poziom rośnie szkoleniem (okno rycerzy, `docs/OKNO_RYCERZY.md`), każdy poziom podwaja siłę |
| 10 | Drwal | siekiera | ścina drzewa → pnie |
| 11 | Tracz | piła | tartak: pień → deska |
| 12 | Hutnik | – | huta żelaza (ruda żelaza + węgiel → stal) i huta złota (ruda złota + węgiel → sztabka) |
| 13 | Kamieniarz | kilof | rozbija skały → kamień |
| 14 | Leśniczy | – | sadzi drzewa |
| 15 | Górnik | kilof | kopalnie kamienia, węgla, żelaza, złota (jedzenie, `docs/ZGODNOSC.md`) |
| 16 | Szkutnik | młotek | stocznia: deska → łódź |
| 17 | Narzędziowiec | młotek + piła | deska + stal → narzędzie (wg priorytetów narzędzi) |
| 18 | Płatnerz | młotek + obcęgi | węgiel + stal → miecz, potem tarcza |
| 19 | Rybak | wędka | łowi ryby w wodzie przy chacie rybaka |
| 20 | Hodowca świń | – | chlewnia: zboże → świnie |
| 21 | Rzeźnik | tasak | rzeźnia: świnia → mięso |
| 22 | Rolnik | kosa | sieje i zbiera zboże |
| 23 | Młynarz | – | młyn: zboże → mąka |
| 24 | Piekarz | – | piekarnia: mąka → chleb |
| 25 | Geolog | młotek | szuka złóż w górach, stawia znaczki |
| 26 | Wolny osadnik | – | czeka w magazynie na zawód; z niego powstają wszyscy pozostali (i rycerze) |
| 27 | Razem | | wszyscy osadnicy gracza |
| 28 | EXIT | | zamyka okno |

Na zrzucie: 190 wolnych osadników z 232 – zapas ludzi jest; brakuje zwykle narzędzi.
