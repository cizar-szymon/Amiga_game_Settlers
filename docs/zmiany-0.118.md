# Zmiany 0.118 (5.11.2026) – lista do dalszych poprawek

Wydanie 0.118 (commit `c62c99e`, łatka Freeserf `0049`). Szczegóły techniczne każdej zmiany:
`docs/PORTING.md`, sekcja „Zmiany 0.118”.

Stan: ✅ działa (sprawdzone przez użytkownika), ⚠️ nie sprawdzone przed wydaniem albo
sprawdzone tylko częściowo.

## Opcje oryginału (Options → Video)

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Dwuklik otwiera okno budowy tylko wtedy, gdy widać samą mapę (bez otwartych okien) | ✅ |
| 2 | Pathway scrolling (mapa idzie za końcem budowanej drogi, tylko tyle, ile trzeba) i Fast building – na początku listy Video | ✅ |
| 3 | Fast mapclick: dwuklik prawym na mapie otwiera okno mapy | ✅ |
| 5 | Dwuklik lewym na własnej fladze zaczyna od niej drogę (Fast building) | ✅ |
| 7 | Dwuklik w miejscu tylko na flagę od razu stawia flagę | ✅ |
| 14 | Dwuklik na pniu ściętego drzewa otwiera okno budowy | ✅ |

## Mysz jak w oryginale

| Nr | Zmiana | Stan |
|---|---|---|
| 4 | Prawy przycisk nie zamyka okna budowy | ✅ |
| 10 | Podgląd miejsc budowy: lewy przy wciśniętym prawym na przycisku budowy w panelu | ✅ |
| 11 | Burzenie tylko lewym przy wciśniętym prawym (sam prawy nie burzy) | ✅ |
| 12 | Klawisz 2 nie burzy | ✅ |
| 19 | Okno budynku: lewy przy wciśniętym prawym na budynku; dwuklik i sam prawy go nie otwierają | ✅ okno własnych budynków; ⚠️ okno ataku na budynek wroga nie sprawdzone |
| 21 | Przewijanie prawym bez końca, wskaźnik stoi w miejscu wciśnięcia (też Invert scrolling) | ✅ |

## Gra

| Nr | Zmiana | Stan |
|---|---|---|
| 6 | Podgląd miejsc budowy pokazuje też miejsca na samą flagę | ✅ |
| 8 | Okno mapy pamięta wybory (tereny, drogi, budynki, siatka, skala), także po ponownym uruchomieniu | ✅ |
| 9 | Autosave: także 3 i 5 min | ✅ |
| 13 | Po zburzeniu budynku bez drogi znika jego flaga (zmiana zasad gry – w LAN obaj muszą mieć 0.118) | ✅ |
| 17 | Bez napisu „TEST 0.xxx” w rogu mapy | ✅ |
| 18 | Mapy wielkości 1 (32×32) i 2 (64×32) | ✅ |
| 20 | Wybrana wielkość mapy zostaje po wyjściu z gry (osobno nowa gra i LAN) | ✅ |
| 26 | Load game: szeroka lista zapisów, bez podglądu mapy | ⚠️ |
| 27 | Szybki zapis Ctrl+Z: krótka nazwa `quick-<rrmmdd>_<ggmmss>.save` (długie nazwy ucinał dysk Amigi i zapis nie był widoczny na liście) | ⚠️ |
| 28 | Okno Save Game 2,5 raza szersze (całe nazwy plików) | ⚠️ |

## Gra LAN

| Nr | Zmiana | Stan |
|---|---|---|
| 15 | Zmiana mapy w menu nie zmienia graczy (drugi gracz zostaje człowiekiem, AI jak ustawione) | ✅ |
| 16 | Show ping in LAN game (Options → Advanced): ping na lewo od FPS | ✅ |
| 22 | Poczekalnia po połączeniu: serwer wybiera mapę (SMALLER / LARGER / OTHER MAP / SAVED GAME), obaj widzą podgląd, czat, START GAME na serwerze | ✅ |
| 23 | W menu startowym przy LAN game mapa się nie generuje (czarne pole z opisem) | ✅ |
| 24 | Szybkie odpowiedzi gracza, który dołączył: CHANGE / AVERAGE / GREAT | ✅ |
| 25 | Zapis przy końcu gry LAN (wyjście gracza, zerwane połączenie): `LAN_ERR_<rrmmdd>_<ggmmss>.lan.save` u obu | ⚠️ krótka nazwa nie sprawdzona (długą ucinał dysk Amigi) |
| 29 | Przy rozjeździe (OUT OF STEP) zapis obu stron `SYNC_ERR_<rrmmdd>_<ggmm>M/S.lan.save`; raport do plików (.txt, wysyłanie do serwera, Ctrl+Shift+D) wyłączony – pójdzie na serwer zgłoszeń | ✅ |
| 30 | Resync: po rozjeździe gra serwera idzie do drugiego gracza, obie grają dalej zgodnie; Ctrl+Shift+R na życzenie; LANTEST + Ctrl+Shift+X – sztuczny rozjazd do testu | ✅ |

## Do zrobienia / sprawdzenia

- ⚠️ Zmiany 25, 26, 27, 28 – sprawdzić na Amidze.
- ⚠️ Zmiana 19 – okno ataku na budynek wroga (lewy przy wciśniętym prawym).
- ✅ Amiga 1 – zaktualizowana do 0.118 (program i szuflada `doc`, 5.11.2026; `Settlers-test` tam nie było).
- Na Amidze 2 w `freeserf/saves` leżą pliki z uciętymi nazwami (`lanend-1978-…lan.`, `LAN_ERR_1978-01-01_…lan`) – gra ich nie widzi; do zmiany nazwy albo usunięcia (za zgodą).
- Plan „BUG LAN”: wysyłanie zgłoszeń na serwer użytkownika (RPi4), gdy będzie postawiony; kod przesyłania plików i `tools/lan_diff.py` są gotowe.
- Uboczne znalezisko: AI przy szukaniu flag do połączenia przegląda zakres według numeru wielkości mapy zamiast liczby pól (`ai-core.cc`, ~linia 1481) – nie zmieniane, bo zmieniłoby grę komputera.

## 0.119 (5.11.2026) – gra LAN, odświeżanie po poleceniach

Wydane na prośbę użytkownika (łatka Freeserf `0050`); potwierdzone przez użytkownika na Amidze 1 + WinUAE. Amiga 1 zaktualizowana do 0.119.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Po postawieniu flagi kursor i panel pokazują flagę, gdy tylko polecenie się wykona – można ją od razu zburzyć | ✅ |
| 2 | Suwaki i przyciski w oknach ustawień przestawiają się po jednym kliknięciu (okno przerysowane po wykonaniu polecenia) | ✅ |
| 3 | Dźwięk przy wysłaniu geologa, awansie na rycerzy i łączeniu dróg mówi, czy się udało | ✅ |

## 0.121 (6.11.2026)

Łatka Freeserf `0052`; testy rozjazdu LAN na gałęzi `lan-desync` (boty, sumy całego stanu, Amiga 1 + WinUAE, `lansim` na PC).

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Save Game: nazwa najwyżej 25 znaków (LAN 21) – dysk ucinał dłuższe | ⏳ do sprawdzenia |
| 2 | Zapis przy rozjeździe: `SYNC_<rrmmdd>_<ggmmss>M/S` (z sekundami) | ⏳ do sprawdzenia |
| 3 | Przyciski + i − powtarzają się przy przytrzymaniu | ✅ |
| 4 | Zapisy zwykłe i LAN osobno (Load game, Save Game, okno LAN) | ✅ |
| 5 | Okno LAN: SAVED GAME → LOAD GAME | ✅ |
| 6 | Okno serwera LAN: wiersze rzadziej | ✅ |
| 7 | Okno LAN → LOAD GAME: DELETE z pytaniem | ✅ |
| 8 | Shift+klik: kilka zapisów, kasowanie naraz | ✅ |
| 9 | Okno LAN szersze, całe nazwy zapisów; widok INTERNET GAME (nieczynny) | ✅ |
| 10 | LAN: najmniejszy wolny numer nowych obiektów – gry zgodne po resyncu i z zapisu | ✅ Amiga 1 + WinUAE (14 min bez różnic, resync co minutę) |
| 11 | Rysowanie mapy nie zmienia gry (dźwięki, ogień) | ✅ jak 10 |
| 12 | Spóźnione „zburz” nie wywraca gry; EINTR nie zrywa połączenia | ✅ `lansim` PC |
| – | Zerwanie połączenia przy resyncu tylko z WinUAE (ECONNRESET) | ⚠️ otwarte – sprawdzić na dwóch Amigach |

## 0.120 (6.11.2026)

Łatka Freeserf `0051`; wszystko potwierdzone przez użytkownika na Amidze 1 (`Settlers-test`).

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Po zmianie gry (koniec LAN, nowa gra, wczytanie) nie zostaje niewidoczne okno pod mapą – Fast building i Fast mapclick działają od razu, bez Esc | ✅ |
| 2 | Dwuklik i prawy klik na oknie, panelu, komunikacie nie przechodzą na mapę pod nim (np. suwak nad flagą) | ✅ |
| 3 | Budowa drogi: zwykły klik na polu innej drogi (w miejscu na flagę) dołącza do niej; dwuklik zaraz potem nie zaczyna nowej drogi | ✅ |
| 4 | Options → Video: „Dead trees on new maps” (0–100 %), w LAN ustawia serwer; 0.120 nie łączy się z 0.119 | ✅ |
| – | Kreska czarnych pikseli przy brzegu wody (stała, w obu trybach rysowania) – zostawiona na prośbę użytkownika, opis w `PORTING.md`, zapis `water-line.save` na Amidze 1 | ⚠️ otwarte |
