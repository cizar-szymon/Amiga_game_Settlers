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

## 0.124 (6.11.2026)

Łatka Freeserf `0055`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Raport F10 podaje miejsce widoku i wskaźnika; opcja `VIEW=kol,wiersz` (z `LOAD=`) | ✅ WinUAE |
| 2 | Kreska czarnych kropek przy brzegu wody: kafelki terenu rysowane od dwóch wierszy mapy wyżej (trójkąt wiersza wyżej na wysokości 0 kończył się w pierwszym wierszu kafelka) | ✅ WinUAE (zrzut przed/po) |

## 0.123 (6.11.2026)

Łatka Freeserf `0054`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Options → Video „Terraforming wasteland” (wył.): leśnik sadzi najpierw na piasku, piasek wokół drzewa (7 pól + pas do granicy) → trawa na stałe | ✅ |
| 2 | Pathway scrolling usunięte z okna Video, zawsze włączone | ✅ wydane |
| 3 | Terraforming: drwal najpierw suche drzewa (2 uderzenia, znika, bez drewna), kamieniarz rozbija brązowe skały (bez kamienia); w LAN ustawienie serwera | ✅ |
| 4 | Podpowiedzi opcji Video: lewy przycisk na nazwie opcji – opis, puszczenie zamyka | ✅ wydane |

## 0.122 (6.11.2026)

Łatka Freeserf `0053`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Gra LAN: Quit → YES → pod spodem „Do you want to save the game?” (YES – okno Save LAN, potem koniec; NO – koniec); żadna strona nie robi `LAN_ERR`; druga strona: „The server/player … ended the game.” (QUIT nie ginie już razem z zamknięciem połączenia) | ✅ |
| 2 | Nazwy LAN na początku PLAYER 1 / PLAYER 2, wpisane zostają | ✅ wydane (bez osobnego testu) |

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

## 0.125 (7.11.2026)

Łatka Freeserf `0056`; potwierdzone przez użytkownika na Amidze 1 (`Settlers-test`), jeśli nie zaznaczono inaczej.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Menu startowe: przy LAN game bez „Mapsize” i przycisków rozmiaru mapy | ✅ |
| 2 | Języki polski i niemiecki (własny czytnik `.po`, szuflada `po`), litery z ogonkami i umlautami dorysowane | ✅ |
| 3 | Język pierwszą pozycją szerokiego okna Options; domyślnie angielski, zapamiętany | ✅ |
| 4 | Podpowiedzi wszystkich opcji (Options, Advanced, Video, Audio); okno rośnie dla długiej podpowiedzi | ✅ |
| 5 | Brakujące znaki czcionki (`, ; ( ) / ' " ! + = _ # * & [ ] < >`) | ✅ |
| 6 | Audio: Paula / AHI (AHI przez interfejs biblioteki, 2 kanały L/P, pierścień 4 × 23 ms) | ✅ czysto, wszystkie dźwięki |
| 8 | Options → Font: Original / Readable (czcionka 8×8 z małymi literami) | ⏳ do potwierdzenia |
| 10 | Przyciski Advanced, Video, Audio, Amiga w Options; Exit wraca do Options | ✅ wyjście poprawione |
| 11 | Volume działa (było puste `apply()`), kroki 10, 0–100 | ✅ |
| 12 | Music, Sound effects, Volume w Audio + osobna głośność muzyki i efektów | ✅ |
| 13 | Tryb 640×512 (320×256 nie – okna gry się nie mieszczą) | ⏳ do potwierdzenia |

## 0.126 (7.11.2026)

Łatka Freeserf `0057`. Logika sprawdzana porównaniem zapisów stara/nowa wersja (zapis 20-last 1000 kroków, gra AI 30 000 kroków, mapy).

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Okno Amiga: WarpTool w kilku miejscach, bez niego Warp z listy kart (`expansion.library`) | ✅ |
| 2 | Zegar CPU co 1 MHz tylko dla MC68060 rev. 6 | ✅ |
| 3 | Postęp tworzenia mapy w menu (procent, pasek), mapa liczona przy widocznym menu | ✅ |
| 4 | Mapy: start bierze mapę podglądu; pole mapy 8 B; generator w osobnych tablicach | ✅ zgodne |
| 5–8 | Logika dużej gry: test magazynów przed szukaniem, części sieci dróg, kolejka szukania, nadrabianie kroków przez czas klatki | ✅ zgodne |
| 9 | Options → Video → Large games (Smooth / Balanced / Full speed) | ✅ |
| 10–11 | Logika: kierunki bez dzielenia, postacie czytane raz, mniejsza flaga, stałe tablice `static const` | ✅ zgodne; 20-last 35 → 10,9 ms/krok, 4,5 → 30 kl./s |
| 12 | Podglądy map zapamiętane przy przełączaniu typu gry | ✅ |

## 0.127 (7.11.2026)

Łatka Freeserf `0058`; obie zmiany potwierdzone przez użytkownika na Amidze 1.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Pauza: polecenia rozgrywki odrzucane (budowa, drogi, burzenie, geolog, atak, magazyny, rycerze, ustawienia) – `LanCommand::is_play`, także w LAN | ✅ |
| 2 | Pauza: cała logika stoi (`Game::update` kończy się po liczniku klatek) – przeciwnik nic nie robi | ✅ |

## 0.129 (7.11.2026)

Gałąź `experimental-speed` (0.128 SPEED, 0.129 SPEED) scalona i wydana jako 0.129; łatki Freeserf `0059`, `0060`. Szczegóły: `PORTING.md`, „Gałąź experimental-speed”. Dane pomiarów: `docs/speed/`, raport PDF: `tools/speed_report.py`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Pętla postaci po tablicy stanów, bez listy indeksów co krok | ✅ zgodne (zapisy, gra AI 30 000 kroków, `-V`) |
| 2 | Tablica obrazków przed tablicą haszującą, skrót z całego id | ✅ obraz identyczny |
| 3 | Sprite'y do VRAM: kopia bez pętli bajtowych, dekodowanie, asembler 68k, krótka ścieżka `draw_image` | ✅ obraz identyczny |
| 4 | Profiler próbkujący `PCSAMPLE` / `simtest -S`, `tools/pcprof.py` | ✅ |
| 5 | Rysowanie blokami 16×16 (`VIEWDRAW=BLOCKS`) – próba bez zysku, wyłączone | – |
| | Wynik (20-last, Full speed): 640×360 23 → 37,9, 800×600 10,1 → 20,1, 960×540 9,6 → 18,2 kl./s; obraz 0.125 ↔ 0.129 identyczny (20 par zrzutów) | ✅ |

## 0.130 (7.11.2026)

Łatki Freeserf `0061`, `0062`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1–2 | Terraforming wasteland: drwale ścinają palmy, palma pada (obrócona palma z gry), pniak | ✅ |
| 3 | Terraforming wasteland: kaktusy i kości znikają z zazielenionego piasku | ✅ |
| 4 | Postęp wczytywania zapisu (okienko z paskiem), wczytywanie szybsze (20-last 8,1 → 5,8 s) | ✅ |
| 5 | LAN: odbudowa stanu po 16 ramek na krok (z testów nocnych) | ✅ |

## 0.131 (8.11.2026)

Łatka Freeserf `0063`.

| Nr | Zmiana | Stan |
|---|---|---|
| 1 | Serwer gry na Pi (`server/`): gry przez internet, zgłoszenia, strona w sieci domowej | ✅ |
| 2–3 | Gra: INTERNET GAME (lista, kod, hasło), `lantest relay` | ✅ (Amiga 1 ↔ WinUAE) |
| 4 | Zgłoszenia z gry: okno F10, rozjazd LAN – zapisy obu stron | ✅ |
| 5 | `RELAYAUTO` (test) | ✅ |
| 6 | Strona serwera: IP, ping, Details / zip / Delete | ✅ |
| 7 | Czytelna czcionka w polach i oknie LAN | ✅ |
| 8 | Większe okno zgłoszenia, opis do 1000 znaków, szybsza wysyłka | ✅ |
| 9 | Opisy dla graczy i programistów, README | ✅ |
