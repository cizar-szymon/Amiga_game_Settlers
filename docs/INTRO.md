# Intro z Amigi (Blue Byte 1993) – jak jest zbudowane

Rozpoznanie (faza 0) przed napisaniem własnego odtwarzacza intro w naszym porcie. Źródło: pliki
z dysku 1 (`data/disk1`, „The Settlers 1”, dysk Intro) i z dysku 2 (`data/disk2`), bo intro
ciągnie się na drugi dysk. Dane gry – nie trafiają do repozytorium ani do wydań.

Oznaczenia: **[spr.]** sprawdzone (kod 68k przeczytany i/lub dane rozpakowane i obejrzane),
**[zał.]** założenie / wniosek niesprawdzony. Adresy `SD00 $xxxx` to przesunięcia w rozpakowanym
module (tak jak w `build/intro/asm/*.s`).

## 1. Jak intro startuje

| Krok | Co | Stan |
|---|---|---|
| `s/startup-sequence` | `run >nil: <nil: loader`, `endcli` | [spr.] |
| `Loader` (316 B, hunk) | `Execute("The Settlers 1:mcp SCPT")`, potem `Execute("The Settlers 3:TheSettlers -f")` | [spr.] |
| `mcp` (8320 B) | spakowany plik wykonywalny (stub + LZ wstecz w stylu Imploder) – rozpakowany w emulatorze (`mcp_unpack.py`) do 12 836 B | [spr.] |
| mcp | „MASSIVE Developments, Oct. 93, Ingo Frick, Alexander Jorias” – jądro modułów (Master Control Program): wczytuje skrypt `SCPT`, ładuje moduły, daje funkcje: pamięć, pliki, przerwania | [spr.] |
| `SCPT` | tekstowy skrypt MASSIVE: `Link $0000 Module "INIT"` → `Link $0001 Module "SD00"` → `Link $9999 Module "EXIT"`. Cała treść intro jest w module **SD00** | [spr.] |

`Intro`/`Introecho`/`Install` to tylko skrypt i małe programy Workbencha (napis, instalacja na HD).

### Funkcje mcp używane przez moduły (a6 = baza mcp)

| Offset | Działanie | Stan |
|---|---|---|
| -$18 / -$1c | otwórz / zamknij moduł po nazwie (d0 = 'DISP' itd.) | [spr.] |
| -$28 / -$30 | przydział / zwolnienie pamięci (d1 = typ, 5 = Chip) | [spr.] |
| -$60 | rozmiar pliku (d0 = nazwa 4-znakowa) → d1 | [spr.] |
| -$54 | wczytaj plik (d0 nazwa, a0 bufor, d1 rozmiar) – w pamięci jest już **rozpakowany z RLE** | [spr.] (SD00 czyta od razu tablice przesunięć) |
| -$98 / -$9c | dodaj / usuń serwer przerwania (d0 = 5 VERTB, 4 COPER, $a = AUD3); struktura: +8 kod, +$c dane (do a1) | [spr.] |
| -$c8 | czy plik istnieje (`FDD2` = drugi dysk / instalacja na HD) | [spr.] |
| -$34 / -$38 | wypełnij (a0, d0 bajtów, d1) / kopiuj (a0 → a1, d0 bajtów) | [spr. w emulatorze] |
| -$40 | przydział pamięci (wariant używany przez USER) | [spr. w emulatorze] |
| -$64, -$48, -$44, -$50/-$4c | zasoby nazwane: wczytaj plik i zarejestruj / czy wczytany / adres / zwolnij | [spr. w emulatorze] |
| -$80 / -$84 / -$88 | lista węzłów USER: dodaj / usuń / znajdź po kluczu | [spr. w emulatorze] |

Wszystkie funkcje zwracają powodzenie w znaczniku Z (Z = 1 OK); -$60 nie zmienia d0.
„Sprawdzone w emulatorze” = nasza implementacja tej funkcji wystarcza, żeby oryginalny kod
przeszedł całe intro z poprawnym obrazem (rozdz. 5).

Jak mcp rozpoznaje, który plik jest w RLE, a który nie (PICx), **nie wiemy** [otwarte].

## 2. Kompresja plików

**RLE (prawie wszystkie pliki dysku 1 i 2)** [spr.]: bajt 0 = znak ucieczki E; dalej `E n v` →
(n+1) × bajt v, każdy inny bajt kopiowany. Bez nagłówka z rozmiarem. `tools/intro/introlib.py:unrle`.
Sprawdzone m.in. na kodzie modułów (`2c79 0000 0004` = `movea.l 4.w,a6` po rozpakowaniu), na
`SCPT` (czytelny tekst) i na bankach próbek (rozmiar w nagłówku = rozmiar po rozpakowaniu).

**ByteRun1 / PackBits (tylko PIC0, PIC2, PIC3, PIC4, PIC6)** [spr.]: pliki PICx **nie** są w RLE.
n = 0..127 → n+1 bajtów dosłownie; n = -128 → nic; n < 0 → następny bajt -n+1 razy. Koniec po
osiągnięciu rozmiaru podanego w skrypcie USxx (PIC0: $19000 = 102 400 B). Dekoder: DISP $111c
(DISP -$4c), wołany przez USER (komenda 4). PIC0 daje dokładnie 102 400 B, PIC6 51 200 B.

## 3. Moduły MASSIVE

Format modułu [spr.]: długie słowo = rozmiar tablicy skoków H, potem `bra.w` co 4 bajty. Baza
modułu (a6) = początek + H, więc funkcja pod przesunięciem X w pliku to `-(H-X)(a6)`
(`tools/intro/modtab.py`). Zawsze: -$14 = zwolnienie (zwraca rozmiar), -$10 = inicjalizacja.
Moduł pod -4(a6) zapisuje swój numer wersji. Adres bazy mcp jest wpisywany w miejsca
`movea.l $0.l,a6` przy ładowaniu.

| Moduł | Rola | Stan |
|---|---|---|
| INIT | pętla komunikatów skryptu (`STOP`, `IN..`), uruchamia kolejne Linki | [spr.] |
| **SD00** (27 800 B) | **całe intro**: kolejność scen, ruch postaci, tablice efektów, mikser próbek, prośba o dysk | [spr.] |
| DISP | ekran: struktura wyświetlania, copper (32 kolory), ByteRun1, blit BOB-ów blitterem (-$54), maski | [spr.] |
| USER | odtwarza plik sceny USxx (otwiera ekran, paletę, wczytuje PIC przez DISP) | [spr.] |
| EFEC | zanikanie palety 32 kolorów do / z czerni ($bc8) lub bieli ($c08); SD00 podaje 16 kroków | [spr.]; czas kroku [zał.] |
| SOUN | odtwarzacz muzyki = ProTracker 2.x obcięty do kanałów 0–2, plus API efektów (nieużywane przez SD00) | [spr.] |
| INPT | wejście (mysz/klawiatura) – SD00 go otwiera, ale **nie używa** | [spr.] |
| FUNC | drobne funkcje pomocnicze EFEC/USER | [zał.] |
| FHRS | czcionka 8×8 (1024 B) – tylko napis „PLEASE INSERT DISK” | [spr.] |
| EXIT | na dysku 3, kończy skrypt | [spr.] |

## 4. Formaty danych

### Struktura wyświetlania (w USxx i w SD00, 46 B) [spr.]

| Off | Znaczenie |
|---|---|
| +0 | następna (przesunięcie) |
| +4 | bajtów na wiersz (jedna płaszczyzna) |
| +6 | wysokość bitmapy |
| +8 / +a | widoczna szerokość / wysokość |
| +c / +e | przesunięcie x / y |
| +10 | liczba płaszczyzn |
| +12 | flagi ($30 pełny ekran, $34 bufor poza ekranem) |
| +14 | przesunięcie palety (0 = brak) |
| +16… | pola wypełniane w czasie działania (wskaźnik bitmapy, tablica wierszy +1a, copper +1e) |

Ekran: **LowRes PAL 320 × 256**, 5 płaszczyzn (32 kolory) albo 6 płaszczyzn **Extra Half-Brite**
(kolory 32–63 = 0–31 o połowę ciemniejsze). Bitmapy zawsze **przeplatane** (w wierszu
płaszczyzna 0, 1, …). Kolory: 12-bit `0RGB`, na 8 bitów ×17.

### USxx – plik sceny [spr.]

| Off | Znaczenie |
|---|---|
| +0 | przesunięcie struktury wyświetlania |
| +2 | przesunięcie skryptu |
| +4 | 'FHRS' (czcionka) |
| +14 | 'SPR1', indeks +1c |
| +22 | nazwa pliku GA, wskaźnik w czasie działania +26 |
| +2c | własna nazwa |
| +32 | lista plików do wczytania: słowo flag + nazwa 4 B, koniec = 0 |

Skrypt: łańcuch elementów (+0 następny, podlisty +e/+10/+12), komenda +6 (USER $1b6):
0 = blit spakowany (DISP -$50), 4 = rozpakuj obraz (nazwa, rozmiar; DISP -$4c), 8 = siatka
kafelków, c = blit sprite'a (DISP -$54). W praktyce US00/02/03/04/06 tylko ustawiają ekran,
paletę i tło. **US05 jest nieużywany** (wskazuje PIC5 i GA05, których nie ma na dyskach).

### PICx – tła [spr.]

ByteRun1 bitmapy przeplatanej, wymiary i paleta z USxx. PIC0 320×512×5 (niebo z promieniami
słońca nad leśną drogą – przewijane), PIC2 gospodarstwo (bitmapa szerokości 480 – przewijanie
w bok w scenie 3), PIC3 kuźnia, PIC4 zamek i rzeka, PIC6 napis „The Settlers – Blue Byte 1993”.

### PIBx – tła EHB [spr.]

RLE, surowa przeplatana bitmapa 320×256×6 (EHB), 61 440 B. Paleta w SD00: PIB1 $224a, PIB5
$3cf4. PIB1 = widok wioski przez dziurkę od klucza, PIB5 = brama.

### GAxx – archiwa BOB-ów i animacji [spr.]

RLE; długie słowo = rozmiar całości, potem długie przesunięcia (pierwsze przesunięcie / 4 =
liczba pozycji + 1). Pozycje **bez nagłówka**: wymiary są wpisane w kodzie SD00 przy wywołaniu
blitu (d3 = szerokość w pikselach, d4 = wysokość). Szerokość w pamięci = ceil(w/16)·2 bajty na
płaszczyznę, zwykle 1 wiersz zapasu. Maska = 1 płaszczyzna tych samych wymiarów; układ albo
obraz/maska na przemian, albo blok obrazów, potem blok masek (GA04).
Blit: DISP -$54 ($131c): d0 obraz, a1 maska lub 0, d1 x, d2 y, d3 szer., d4 wys., a0 ekran.

| Plik | Płaszczyzny | Zawartość (z PNG) |
|---|---|---|
| GA00 | 5 | woda, króliki, rycerz na koniu |
| GA01 | 6 (EHB); pozycje 34–45: 5 płaszczyzn, 80×150 obraz/maska do sceny z bramą | kafelki, rycerz od tyłu |
| GA02 | 5 | kot na płocie, kowal |
| GA03 | 5 | różnice klatek – młot kowala (8 klatek) |
| GA04 | 5 | rycerz, zwodzony most; różnice klatek wody |
| GAB2 | 5 | czterech biegnących chłopców i pies |
| GAC2 | 5 | rycerz na koniu 160×189 |
| GA06 | – | pusty (8 B) |

**Animacja różnicami (GA03 0–7, GA04 0–3)** – SD00 $6076 [spr.]: rekordy 24 B: przesunięcie
w bitmapie ekranu (4 B), potem 5 długich słów zapisanych pod przesunięciem, +40, +80, +120, +160
(32 piksele każdej płaszczyzny w jednym wierszu); koniec = -1. Zapis nadpisuje (nie XOR), na tle
PIC. GA03: klatka = licznik & 7.

### KPIC / KMSK [spr.]

RLE; KPIC = przeplatany obraz 208×126×5, KMSK = jego maska (1 płaszczyzna). Trzy klatki po
42 wiersze jedna pod drugą (rycerz na koniu). SD00 $5e1a wycina okno 46×42 w x = $a2 − 2n,
klatka (n mod 3)·42.

### SPR1 – wskaźnik myszy [spr.]

Sprzętowe sprite'y 16 kolorów, 16×11, punkt 5,5. Dla odtwarzacza bez znaczenia.

### SMPx / SML2 – banki efektów dźwiękowych [spr.]

RLE; +0 rozmiar całości, potem przesunięcia próbek 1..N i przesunięcie końca. Każda próbka to
pełny plik **IFF 8SVX** („Audio Master II”, 8363 Hz, 8 bit ze znakiem); dane BODY od +$68.
`tools/intro/samples.py` → `build/intro/wav/` (31 plików WAV: ptaki, woda, koń, kot, dzieci,
pies, kowal, ogień, most, okrzyk, walka).

**Mikser efektów w SD00** [spr.]: 3 głosy programowo zmiksowane na **kanał 3 Pauli**, okres
$1bf → **7935 Hz**, bufory po 160 B (= jedna ramka 50 Hz), przerwanie AUD3 ($50a) przełącza
bufory, mikser ($558) liczy następny. Głośność: tablica 64×256, próbka·vol/64/3. Próbki 8363 Hz
grane bajt po bajcie przy 7935 Hz (trochę niżej niż oryginał – tak było). Odtworzenie ($41e):
d0 = numer próbki, d1 = głośność 0..$3f, d2 = liczba powtórzeń ($ffff = pętla); bierze pierwszy
wolny z 3 głosów, gdy wszystkie zajęte – nic nie gra.

**Harmonogram efektów sceny** [spr.]: wpisy po 8 B `tick, próbka, głośność, powtórzenia`;
co tick sprawdzany jeden wpis ($3d6). `tools/intro/scenes.py` wypisuje wszystkie tablice.

### MUSI – muzyka [spr.]

Po RLE to **zwykły moduł ProTrackera (M.K., 31 instrumentów)**, 95 686 B: tytuł „nintro5”,
długość utworu 15, kolejność 0..13,13, 14 wzorów. Używa tylko 3 kanałów (4. pusty) i tylko
efektu C (głośność); prędkość 6, bez F → **120 ms na wiersz, 115,2 s na pętlę, gra w kółko**.
Odtwarzacz w SOUN: ProTracker 2.x z VBlanka 50 Hz, tylko kanały 0–2 (3 jest dla miksera
efektów), wszystkie głośności przez tablicę głośności głównej (zanikanie: 65 ramek), na końcu
utworu powrót do 0, przy starcie wyłącza filtr audio (bit 1 $bfe001).
SD00 włącza muzykę w scenie 1 ($fd6) i gra ją przez wszystkie sceny; na końcu ($ade) zanikanie
~1,3 s i stop. `tools/intro/music.py` → 11 instrumentów WAV i cały utwór
`build/intro/music/intro_song.wav` (render w Pythonie wg kodu – wysoka pewność, bez odsłuchu).

## 5. Sceny i choreografia (z przebiegu oryginalnego kodu w emulatorze) [spr.]

Choreografię zmierzyliśmy, uruchamiając **oryginalny kod** (SD00 + DISP, USER, EFEC, FUNC, FHRS)
w emulatorze 68k na PC (`tools/intro/emu.py`, unicorn) z naszym modelem custom chipów: licznik
promienia, blitter, copper (obraz okna PAL), przerwania VBlank, COPER i AUD3, sztuczne mcp, SOUN
i INPT. Lewy przycisk nie jest wciśnięty, `FDD2` „jest” (bez próśb o dyski). Przebieg daje dla
każdej ramki 50 Hz obraz, listę operacji i stan dźwięku. Klatki z emulatora obejrzane: zgadzają
się z treścią z PNG (przewijanie nieba, rycerz, chłopcy z psem, kuźnia, zamek, brama, napis).
Pełny wykaz: `build/intro/choreography.txt` (`tools/intro/choreo.py`).

Tempo: VBlank ($984) liczy ramki, krok sceny co `$92` ramek (6 → 8,33 kroku/s; scena 4: 7).
Podwójny bufor (SD00 przełącza `$a0`, DISP podmienia listę coppera). Wejście/wyjście sceny:
EFEC, 16 kroków palety. Sceny jedna po drugiej, czasy w ramkach PAL od startu SD00:

| # | Ramki | Czas | Treść | Pliki | Jak się kończy |
|---|---|---|---|---|---|
| 1 | 1–835 | 16,7 s | niebo z promieniami PIC0 (320×512) przewijane w dół 256 linii na leśną drogę; strumień i wodospad (GA00 0–15, klatka co 6 ramek), zwierzątko (GA00 82–90), zając (GA00 16–68), rycerz na koniu (GA00 70–80, 80×80, x 0..198) | US00, PIC0, GA00, SMP0, MUSI | flaga $96 po przejściu rycerza |
| 2 | 835–1985 | 23,0 s | wioska przez dziurkę od klucza (PIB1, EHB), animacje GA01 0–31 (klatka co 6 ramek), błyski na krawędzi przesuwane blitterem kolumnami płaszczyzn [zał. co przedstawiają], rycerz od tyłu skalowany w pionie przez CPU (SD00 $1f10) | PIB1, GA01, SMP0 | koniec tablicy $1ffe |
| 3 | 1985–3646 | 33,2 s | gospodarstwo 480 px, okno 320 przewijane przez blity; kot na płocie (GA02 0–15), czterech biegnących chłopców i pies (GAB2, przez bufor), postać w oknie (GA02 17–47), na końcu rycerz na koniu 160×189 (GAC2) | US02, PIC2, GA02, GAB2, GAC2, SMP2, SML2 | koniec tablicy $57be (krok 177 = 21 s: od tego momentu działa LPM) |
| 4 | 3646–4264 | 12,4 s | kuźnia PIC3: młot (różnice GA03 0–7, co 7 ramek), ogień, rycerz KPIC/KMSK wycinany oknem 46×42 | US03, PIC3, GA03, KPIC, KMSK, SMP3 | 595 ramek ($90 ≥ $253) |
| 5 | 4264–5511 | 24,9 s | zamek i rzeka PIC4: woda (różnice GA04 0–3), rycerz idzie (GA04 4–9, 37×47), most zwodzony (GA04 17–23, 96×98) | US04, PIC4, GA04, SMP4 | krok $ce = 206 |
| 6 | 5511–6169 | 13,2 s | brama PIB5 (EHB), rycerz od tyłu (GA01 34–45), migające elementy (GA01 46–61) | PIB5, GA01, SMP5 | koniec tablicy $3b80 |
| 7 | 6169–7127 | 19,2 s | napis „The Settlers – Blue Byte 1993” (PIC6): rozjaśnienie (tablica $6762, 14 kroków), 750 ramek | US06, PIC6 | 750 ramek |
| – | 7127–7192 | 1,3 s | zanikanie muzyki (SOUN -$3c, 65 ramek), ekran czarny | | stop muzyki |

**Razem 7127 ramek = 142,5 s**, z wyciszeniem muzyki 143,8 s.

**Efekty dźwiękowe** – dokładne ramki startu w `choreography.txt` (np. scena 1: woda w pętli od
ramki 9, koń od 222, ptaki 147…567; scena 4: kowal co 56 ramek). Źródło tablic: SD00 (harmonogram
$3c4/$3d6, wpisy po 8 B) i wywołania z przerwania VBlank (koń w scenie 1).

**Pomijanie w oryginale** [spr.]: tylko **lewy przycisk myszy** (`btst #6,$bfe001`), tylko
**bieżąca scena**; w scenie 3 dopiero od kroku 177. Klawiatura i joystick nie działają. Prośby
o dysk ($69d6) pokazują się tylko bez `FDD2` – u nas nie występują (wszystkie pliki w jednej
szufladzie).

## 6. Skrypt intro – podstawa odtwarzacza [spr.]

Zamiast przepisywać ręcznie siedem scen (kod SD00 to kilkadziesiąt procedur: przewijanie przez
copper, skalowanie wierszy przez CPU, przesuwanie kolumn błysków blitterem, okna przewijane
blitami, wycinanie KPIC maską), **nagrywamy raz, co oryginał robi z pamięcią obrazu**, i to nagranie
jest danymi odtwarzacza. Nagranie nie zawiera grafiki ani dźwięku – tylko operacje, współrzędne
i palety; grafika i próbki są czytane z oryginalnych plików gracza.

Pamięć: każdy przydział mcp to **blok** (numer, rozmiar). Wskaźniki w nagraniu to `[blok,
przesunięcie]` (przesunięcie może być lekko ujemne – blit zaczynający słowo przed buforem).
Operacje na ramkę (czas `f`; `f - 0,5` = z przerwania, przed mikserem efektów):

| Operacja | Znaczenie |
|---|---|
| `alloc` | nowy blok (zera) |
| `load` | plik (po RLE; PICx surowy) do bloku |
| `fill`, `copy`, `poke` | zapisy CPU: wypełnienie, kopia (z bloku albo stałych modułu, np. czcionka FHRS), dosłowne bajty |
| `unpack` | ByteRun1 (DISP $111c) |
| `delta` | różnice klatek GA03/GA04 (SD00 $6076/$62c8) |
| `blit` | blitter Amigi: BLTCON0/1, BLTSIZE, maski, 4 modulo, wskaźniki A–D, dane A–C |
| `sfxbank`, `sfx`, `sfx_on`, `sfx_off`, `sfx_stop` | mikser efektów SD00 (bank, próbka, głośność, powtórzenia; zatrzymanie głosu $402) |
| `music` | start / wyciszenie MUSI |
| widok (na ramkę) | pasy obrazu: pierwsza linia, wskaźniki płaszczyzn, głębia, BPLCON1, DDFSTRT/STOP, modulo, 32 kolory, DIW |

Zapisy CPU są łapane w emulatorze: każdy zapis do bloku danych porównywany z ostatnimi odczytami
(ta sama wartość → kopia z tamtego adresu; kolejne kopie i bajty łączone w ciągi). Dosłowne bajty
(`poke`) to 73 KB: listy coppera, tablice wierszy DISP, bufory masek – połowa trafia do bloków,
których nic nie czyta. Grafiki w nich nie ma.

**Weryfikacja** (`player_ref.py --check`): odtwarzacz w Pythonie, który zna tylko pliki gracza
i nagranie, odtwarza obraz **identyczny piksel w piksel** z emulatorem we wszystkich 7127 ramkach
(wynik niżej, rozdz. 7) i strumień efektów identyczny poza 8 ramkami na przejściach scen.

Rozmiar nagrania w formacie binarnym (`make_script.py` → `build/intro/intro.script`): **3,1 MB,
po zlib 342 KB** (blity 1,5 MB, kopie 1,07 MB, poke 0,29 MB, widoki 0,13 MB). Da się zmniejszyć:
pominąć zapisy do bloków, których nic nie czyta (połowa poke), kodować blity różnicowo.

## 7. Odtwarzacz wzorcowy (Python) i film [spr.]

`tools/intro/player_ref.py`: bloki w formie planarnej (jak w Chip RAM), blitter (tryb obszarowy,
przesunięcia, maski, wstecz), c2p widoku co ramkę (5 płaszczyzn albo 6 z EHB), mikser efektów
SD00 (3 głosy, tablica głośności `floor(s·vol/64)/3`, suma bajtów z przepełnieniem, 160 próbek na
ramkę ≈ 7935 Hz), muzyka MUSI (ProTracker, `music.py`) od ramki 5 do końca z wyciszeniem 65 ramek.
Wynik: `build/intro/frames/*.png` (320×256), `build/intro/intro_ref.wav` (22 050 Hz stereo:
muzyka kanały 0 lewy, 1–2 prawy; efekty kanał 3 lewy) i **`build/intro/intro_ref.mp4`**
(640×512, 50 kl./s, ffmpeg).

**Wynik `player_ref.py --check` (4.11.2026):** 7127 z 7127 ramek **identycznych** z emulatorem
(obraz RGB porównany piksel w piksel); strumień efektów: 945 z 1 140 320 próbek różnych,
wyłącznie w 8 pojedynczych ramkach na przejściach scen (wyłączenie kanału 3 w trakcie bufora).
Film: `build/intro/intro_ref.mp4`, 143,8 s (7127 ramek + 65 ramek wyciszenia muzyki), 13,5 MB.

**Znane różnice wobec prawdziwej Amigi** [zał. / do sprawdzenia na sprzęcie]:
- Emulator nie liczy czasu procesora dokładnie (300 instrukcji na linię). Sceny są synchronizowane
  przez VBlank, więc czas kroków się zgadza; gdyby na A500 krok nie mieścił się w 6 ramkach,
  oryginał by zwalniał – nasz zapis tego nie pokaże.
- Copper: rysujemy wiersze bez podziału w poziomie (WAIT z pozycją poziomą działa od początku
  następnej linii); sprite'y (wskaźnik myszy SPR1) pominięte.
- Efekty: bufor 160 próbek na ramkę 50 Hz, w nagraniu i w filmie odtwarzany z 8000 Hz (prawdziwe
  7935 Hz → u nas ton o 0,8 % wyżej, bez rozjazdu z obrazem). Moment wejścia nowej próbki
  zaokrąglony do ramki.
- Muzyka: renderer `music.py` (najbliższa próbka, bez filtra Pauli); wyciszenie liniowe zamiast
  tablicy SOUN.
- Na 8 przejściach scen strumień efektów odtwarzacza różni się od emulatora w jednej ramce
  (wyłączenie kanału 3 w trakcie bufora) – niesłyszalne, w czasie wyciemniania.

## 8. Co jest niepewne

- Wierność samego emulatora: blitter i copper napisaliśmy sami (sprawdzone na obrazie wszystkich
  scen, ale nie porównane z WinUAE). Najlepszy test: nagranie intro w WinUAE i zestawienie klatek.
- GA02 pozycje 50–53 (16×32, maski kolumn w scenie 3) i GA01 58–61 (16×8, migające światło
  w scenie 6) – dekodują się dobrze dopiero w kontekście sceny.
- Jak mcp odróżnia pliki RLE od PICx (ByteRun1) – dla nas bez znaczenia.
- Czy nagranie (operacje, bez grafiki) można dołączyć do wydania – patrz rozdz. 9, pytania.

## 9. Plan odtwarzacza w naszym porcie (C++)

**Zasada:** odtwarzacz w C++ robi dokładnie to, co `player_ref.py`: czyta pliki gracza z
`data/intro` (RLE, ByteRun1, 8SVX, MOD) i skrypt intro (`intro.script`), wykonuje operacje na
blokach w Fast RAM, co ramkę zamienia widok na piksele 8-bit i wysyła na ekran RTG. Oryginalnego
kodu 68k nie uruchamiamy.

**Struktura (proponowane pliki w `port/`):**

| Moduł | Zawartość | Rozmiar (szac.) |
|---|---|---|
| `intro_files` | odczyt plików z `data/intro`, RLE, ByteRun1, sprawdzenie kompletu (brak → bez intro) | ~120 wierszy |
| `intro_script` | wczytanie `intro.script` (zlib albo bez), dekodowanie rekordów na ramkę | ~200 |
| `intro_blitter` | blitter Amigi na blokach (słowa 16-bit, przesunięcia, maski, 8 mintermów, wstecz) – C, później asm 68k | ~200 |
| `intro_ops` | alloc, load, fill, copy, poke, unpack, delta | ~120 |
| `intro_video` | c2p pasów widoku (5/6 płaszczyzn, EHB, BPLCON1), paleta 64 kolory → CLUT, 2× do 640×512 albo 640×480 z obcięciem 8 linii, wysyłka zmienionych wierszy | ~250 |
| `intro_audio` | mikser efektów SD00 (3 głosy, tablica 64×256) do istniejącego miksera gry; MUSI przez micromod, wyciszenie `micromod_set_gain` | ~180 |
| `intro_main` | pętla 50 Hz (`timer.device`), klawisz / klik / joystick → koniec, `NOINTRO`, sprzątanie | ~120 |

Razem **~1200 wierszy C++** i **~2–3 dni** pracy plus testy na Amidze; choreografii nie trzeba
już rozpisywać. Narzędzie `make_script.py` (jest) daje skrypt; do zrobienia: zmniejszenie skryptu
(pominięcie nieczytanych bloków, blity różnicowo) i test `--check` dla C++ (zrzut ramek
porównany z `player_ref.py`).

**Kolejność:**
1. `intro_files` + `intro_script` + `intro_ops` + `intro_blitter`, test na PC/w Dockerze
   z porównaniem pamięci bloków z `player_ref.py` po każdej scenie.
2. `intro_video` na Amidze: scena 1 (przewijanie) i 7 (napis), pomiar czasu ramki.
3. `intro_audio`: efekty, potem muzyka.
4. Wyjście klawiszem, `NOINTRO`, brak plików → bez intro, `data/intro/ReadMe.txt`.

**Wydajność (szac.):** blity sceny to kilkanaście–kilkaset tysięcy słów na krok (co 6 ramek), c2p
320×256×6 = 61 KB na ramkę; na 68060 kilka ms – pełne 50 kl./s bez problemu, ale zmierzyć.

**Ryzyka:**
- Status prawny skryptu: to zapis operacji oryginalnego programu (bez grafiki, z 37 KB bajtów
  coppera/masek). Jeśli nie chcemy go publikować, alternatywa: narzędzie generujące skrypt
  u gracza (wymaga Pythona + unicorn – mało realne na Amidze) albo ręczne przepisanie scen
  (2–3 dni RE na scenę, mniej wierne).
- Wierność emulatora (blitter/copper) – sprawdzić z WinUAE.
- Rozmiar skryptu w pamięci (3 MB bez kompresji; można czytać po kawałku albo trzymać zlib).

## 10. Decyzje użytkownika (4.11.2026)

- **Własny odtwarzacz** w porcie (nie uruchamiamy oryginalnego kodu OCS).
- **Ekran:** tryb 640×512, jeśli karta go ma (obraz 2× bez strat); inaczej 640×480 z obrazem 2×
  i obciętymi 8 liniami u góry i na dole (proporcje bez zmian).
- **Pliki:** szuflada `Settlers/data/intro` (z `ReadMe.txt`, co skopiować z własnych dysków 1 i 2);
  gdy plików brakuje, gra startuje od razu jak dotąd, bez komunikatu.
- **Kiedy:** przy każdym starcie; dowolny klawisz albo kliknięcie od razu przechodzi do gry
  (zachowanie naszego portu, oryginał miał tylko lewy przycisk dla bieżącej sceny).
  Tool Type / opcja `NOINTRO` wyłącza intro.

## 11. Narzędzia (`tools/intro/`, wynik w `build/intro/`, poza gitem)

| Skrypt | Co robi |
|---|---|
| `introlib.py` | RLE, ścieżki (`data/disk1`, potem `data/disk2`) |
| `unpack_all.py` | rozpakowuje RLE wszystkich plików do `build/intro/raw/` (bez PICx) |
| `mcp_unpack.py` | rozpakowuje `mcp` jego własnym stubem w emulatorze 68k → `raw/mcp_main.bin`, `asm/mcp_main.s` |
| `disasm.py`, `modtab.py`, `show.py` | disassembler (capstone), tablica skoków modułu, wycinek listingu |
| `gfx.py`, `gfx_ga.py` | dekodery obrazów; `python gfx_ga.py` → PNG w `build/intro/png/` |
| `samples.py` | banki SMPx/SML2 → 31 WAV w `build/intro/wav/` |
| `music.py` | MUSI → instrumenty WAV i cały utwór `build/intro/music/intro_song.wav` |
| `scenes.py` | lista scen i harmonogramy efektów z tablic SD00 |
| **`emu.py`** | oryginalne intro w emulatorze 68k (unicorn) z naszym blitterem, copperem, przerwaniami → `build/intro/emu/`: klatki PNG, `log.json`, `timeline.json` (skrypt), `views.json`, `sfx.raw`, `cpu_writes.txt`; ok. 25 min |
| `choreo.py` | opis choreografii scen z `log.json` → `build/intro/choreography.txt` |
| **`player_ref.py`** | odtwarzacz wzorcowy: pliki gracza + skrypt → `frames/`, `intro_ref.wav`, `intro_ref.mp4`; `--check` porównuje z emulatorem; ok. 15 min |
| `make_script.py` | skrypt → binarny `build/intro/intro.script` dla C++ (format w nagłówku pliku) |

Wymagania: Python 3, Pillow, numpy, `pip install capstone unicorn`, ffmpeg (film).
Kolejność: `emu.py` → `player_ref.py --check` → `make_script.py`, `choreo.py`.

## 12. Pytania otwarte

1. Czy skrypt intro (`intro.script`, ok. 340 KB po zlib: operacje, współrzędne, palety, listy
   coppera, bez grafiki i dźwięku) może być częścią wydania? Bez niego odtwarzacz w C++ wymaga
   ręcznego przepisania scen.
2. Czy przed pisaniem C++ porównać emulator z WinUAE (nagranie prawdziwego intro)?
