# Akceleracja FPGA dla portów gier na Warp 1260 / 7060

Propozycja funkcji blittera i skalera z pomiarami wydajności z sześciu portów.

- Autor: Szymon Ciszewski
- Data: 9 listopada 2026
- Karty: Warp 1260 / 1260 Extreme / 7060
- Pomiary: A1200 + Warp 1260X (060/100 MHz), A4000 + Warp 7060 (060/100 MHz)

---

Sześć gier portowanych na Amigę działa na kartach Warp z Picasso96. Trzy z nich nie
osiągają pełnego tempa w wyższych rozdzielczościach z powodu operacji graficznych,
które mógłby przejąć FPGA karty. Poniżej opis trzech funkcji z konkretnymi pomiarami
i szacowanym przyspieszeniem oraz propozycja interfejsu programistycznego.

## Porty i ich stan wydajności

| Gra | Typ | Obraz | FPS teraz | Cel | Wąskie gardło |
|---|---|---|---|---|---|
| **Settlers** | Port C++ (Freeserf) | 8-bit CLUT | 38 fps (640×360), 18 fps (960×540) | 50 | Sprite'y z przezroczystością do VRAM (40–45% rysowania) |
| **Vastar** | Emulator Z80 | 8-bit → 16-bit | 60/18 rys. (SCALE=2), 60/15 rys. (SCALE=3) | 60/60 | Konwersja palety + zapis podwojonych pikseli (54% CPU przy ×3) |
| **Xain'd Sleena** | Emulator 6809 | 9-bit → 16-bit | 16–20 fps (SCALE=2) | 57 | Konwersja 16–18 ms + zapis VRAM 13–15 ms na klatkę |
| AmiXcom | Port C (OpenXcom) | 8-bit CLUT | 28–37 fps (bitwa) | 50 | Sprite'y z przezroczystością (blitNShade) |
| Wizard of Wor | Przepisanie z C64 | 8-bit CLUT | 60 fps, CPU 36–50% | 60 ✓ | Brak — pełne tempo z zapasem |
| OpenDUNE | Port C (Dune II) | 8-bit CLUT | 57 fps, CPU 25–47% | 57 ✓ | Brak — pełne tempo z zapasem |

Pogrubione gry zyskałyby na akceleracji FPGA.

---

## Funkcja 1: Zoom ×2 (scan doubler) — NAJWYŻSZY PRIORYTET

FPGA podwaja piksele i linie skanujące przy wysyłaniu do monitora. Gra renderuje
w mniejszej rozdzielczości, bufor VRAM jest mały, a monitor wyświetla obraz
w podwójnym rozmiarze. Zerowy koszt CPU.

### Jak działa

Gra otwiera ekran P96 w natywnej rozdzielczości (np. 640×360). FPGA wyświetla go
na monitorze jako 1280×720 — każdy piksel podwojony w obu osiach. CPU nie wykonuje
żadnej dodatkowej pracy.

### Pomiary — ile kosztuje brak zoomu

Dziś gry, które potrzebują większego obrazu, albo rozciągają piksele przez CPU
(×2 = ×4 danych do VRAM), albo renderują w większej rozdzielczości (więcej pikseli
do narysowania). Obie drogi są kosztowne:

| Gra | Natywna rozdzielczość | Koszt CPU ×2 teraz | FPS teraz | FPS z zoomem FPGA |
|---|---|---|---|---|
| Settlers | 640×360 (8-bit) | ~40 ms/kl. (CPU scaling) | 12,5 | **38 → 50** * |
| Vastar | 224×256 (16-bit) | 35 ms/kl. (convert + write ×2) | 14 rys./s | **~36 rys./s** |
| Xain'd Sleena | 256×240 (16-bit) | 37 ms/kl. (convert + write ×2) | 10–14 rys./s | **~28 rys./s** |
| AmiXcom | 320×200 (8-bit) | ~7 ms/kl. (c2p doubling) | 37 (bitwa scroll) | **~42** |

\* Settlers z zoomem + masked blit (funkcja 2) osiąga 50 fps = pełne tempo PAL.

**Zysk:**
- Settlers: **×3** (12,5 → 38 fps przy 1280×720)
- Vastar: **×2,5** (14 → 36 rys./s)
- Xain'd Sleena: **×2** (14 → 28 rys./s)
- AmiXcom: **+14%** (37 → 42 fps)

### Złożoność FPGA

**Mała.** Podwajanie scanlinii i pikseli to standardowy blok (scan doubler). Dotyczy
całego ekranu — nie wymaga osobnych poleceń na klatkę.

---

## Funkcja 2: Masked blit (kopia z przezroczystością + cień) — WYSOKI PRIORYTET

Kopiowanie prostokąta z jednego miejsca pamięci do drugiego z pominięciem koloru
przezroczystego (indeks 0) i opcjonalnym trybem cienia (indeks 1 → odczyt piksela
docelowego i zamiana przez tablicę).

### Co robi CPU teraz

Settlers rysuje 400–840 sprite'ów na klatkę funkcją `draw_runs_vram68k` (asembler
68k). Dla każdego sprite'a iteruje przebiegi zakodowane jako sekwencja (pomiń N
pikseli / kopiuj N pikseli / cieniuj N pikseli):

- **Kopia:** bajty z Fast RAM do VRAM, z pominięciem koloru 0
- **Cień:** odczyt piksela z VRAM (wolne!), zamiana przez tablicę 256 bajtów, zapis
  z powrotem

### Pomiary — Settlers (A1200, 060/100 MHz, wersja 0.131)

| | 640×360 | 960×540 |
|---|---|---|
| FPS | 38,2 | 17,7 |
| Rysowanie na klatkę | 14,7 ms | 32,8 ms |
| `draw_runs_vram68k` na klatkę | **5,9 ms (40% rysowania)** | **14,7 ms (45% rysowania)** |
| → kopia pikseli | 38% | 36% |
| → dekodowanie przebiegów | 26% | 25% |
| → cienie (w tym odczyt VRAM 13–15%) | 32% | 34% |
| Sprite'ów / klatkę | ~385 | ~840 |
| Pikseli kopiowanych / klatkę | ~32 000 | ~73 000 |
| Pikseli cieni / klatkę | ~6 400 | ~16 000 |
| Przebiegów (skip/copy/shade) / klatkę | ~9 800 | ~22 000 |

### Szacowane przyspieszenie

Logika gry to stałe 50 kroków/s (~400 ms/s). Klatki dostają resztę czasu.
Po usunięciu `draw_runs_vram68k` z CPU:

| | 640×360 | 960×540 |
|---|---|---|
| Rysowanie bez sprite'ów / klatkę | 8,8 ms | 18,1 ms |
| Maks. FPS = (1000 − logika) / rysowanie | **68** → 50 PAL | **~34** |
| Było → jest | **38 → 50 fps (+32%)** | **18 → 34 fps (+92%)** |

### AmiXcom — analogiczna sytuacja

Bitwy: sprite'y z przezroczystością (`blitNShade`) to dominujący koszt rysowania.
Masked blit przyspieszyłby bitwy o ~30% (28–37 → ~40–50 fps).

### Wymagania dla FPGA

- **Kopia z kolorem przezroczystym:** piksel == 0 → pomiń, inaczej → zapisz do celu
- **Tryb cienia:** piksel == 1 → odczytaj piksel docelowy, zamień przez tablicę
  256 bajtów (załadowaną raz), zapisz wynik
- **Szybkość polecenia:** 400–840 poleceń na klatkę; narzut na polecenie musi być
  << 10 µs
- **Źródło / cel:** VRAM (oba); źródło mogłoby być też w Fast RAM, gdyby DMA było
  dostępne

### Złożoność FPGA

**Średnia.** Wymaga odczytu piksela docelowego przy cieniu (read-modify-write).
Tryb bez cienia (sama przezroczystość) jest prostszy.

---

## Funkcja 3: Paleta CLUT w FPGA (8-bit → 16-bit) — WYSOKI PRIORYTET

Gra zapisuje do VRAM piksele jako 8-bitowe indeksy palety. FPGA konwertuje je do
16-bit RGB przy wysyłaniu do monitora, używając tablicy 256 (lub 512) kolorów
załadowanej raz.

### Problem

Emulatory automatów (Vastar, Xain'd Sleena) renderują obraz w indeksach palety
(8 lub 9 bitów), ale karta Warpa w tych rozdzielczościach działa w 16 bitach.
CPU musi przeliczać każdy piksel przez tablicę przed zapisem do VRAM — a przy
SCALE=2 robi to dla ×4 pikseli.

### Pomiary

| Gra | Koszt konwersji / klatkę | Udział w „present" | Rozmiar palety |
|---|---|---|---|
| Vastar (SCALE=2) | **17,5 ms** | ~50% | 256 kolorów |
| Xain'd Sleena (SCALE=2) | **16–18 ms** | ~48% | 512 kolorów (9-bit) |

### Szacowane przyspieszenie

Konwersja znika z CPU. W połączeniu z zoomem ×2 (funkcja 1) — CPU zapisuje natywną
rozdzielczość w 8 bitach, FPGA robi resztę:

| Gra | Teraz (SCALE=2) | Z CLUT + zoom | Zysk |
|---|---|---|---|
| Vastar | 14 rys./s | **~50 rys./s** | ×3,5 |
| Xain'd Sleena | 10–14 rys./s | **~40 rys./s** | ×3 |

Vastar przy SCALE=2 na 060/100 MHz osiąga już 60/60 (w 73% CPU). Z CLUT+zoom
na 060/50 byłby bliski 60/60 — **pełne tempo automatu nawet na wolniejszym
procesorze**.

### Złożoność FPGA

**Mała–średnia.** Tablica 256×16 bitów (512 B) lub 512×16 (1 KB) w rejestrach lub
BRAM. Konwersja piksel po pikselu przy skanowaniu — to ten sam tor co scan doubler.
Naturalnie łączy się z funkcją 1 (zoom): FPGA czyta 8-bit z VRAM, zamienia na 16
przez tablicę, podwaja i wysyła.

---

## Podsumowanie priorytetów

**Trzy funkcje pokrywają wąskie gardła wszystkich portów, które nie osiągają pełnego
tempa.** Zoom + CLUT rozwiązuje emulatory (Vastar, Xain). Zoom + masked blit
rozwiązuje Settlers i AmiXcom. Razem dają pełne tempo w 4 z 6 gier (pozostałe 2
już je mają).

| # | Funkcja | Złożoność FPGA | Settlers | Vastar | Xain | AmiXcom | WoW | Dune |
|---|---|---|---|---|---|---|---|---|
| 1 | **Zoom ×2** | Mała | **×3** | **×2,5** | **×2** | +14% | — | — |
| 2 | **Masked blit** | Średnia | **+32%** | — | — | **+30%** | — | — |
| 3 | **Paleta CLUT** | Mała–śr. | — | **×1,5** | **×1,5** | — | — | — |

Mnożnik dotyczy FPS rysowanych klatek przy danej funkcji samej. Funkcje 1+3 razem
dają ×3–3,5 dla emulatorów; 1+2 dają ×4 dla Settlers (12,5 → 50).

---

## Proponowany interfejs

### Zoom ×2 — tag na ekranie (ustawiany raz)

Naturalnie pasuje do P96: gra otwiera ekran w natywnej rozdzielczości, a driver
Warpa włącza skalowanie. Nie wymaga wywołań na klatkę.

```c
/* Opcja A: niestandardowy tag przy OpenScreen */
screen = OpenScreenTags(NULL,
    SA_DisplayID, modeid,
    SA_Width, 640,  SA_Height, 360,
    WARP_SA_Zoom, 2,       /* FPGA wyświetla jako 1280×720 */
    TAG_DONE);

/* Opcja B: osobna funkcja po otwarciu ekranu */
WarpSetZoom(screen, 2);    /* 0 = wyłącz */
```

### Paleta CLUT — załadowanie tablicy (kilka razy na klatkę)

Gra ładuje tablicę konwersji, potem zapisuje 8-bitowe piksele do VRAM. FPGA
konwertuje przy skanowaniu. Aktualizacja palety (zmiana kolorów) wymaga
ponownego załadowania.

```c
/* 256 wpisów × 16 bitów (R5G6B5) */
WarpSetCLUT(screen, rgb16_table, 256);

/* Przy zmianie palety (np. flash, nowy poziom) */
WarpSetCLUT(screen, new_table, 256);
```

### Masked blit — lista poleceń lub funkcja biblioteki

400–840 wywołań na klatkę wymaga niskiego narzutu. Dwie opcje:

#### Opcja A: warp.library (czysty interfejs Amigi)

```c
struct Library *WarpBase = OpenLibrary("warp.library", 1);

/* Kopia z przezroczystością (kolor 0 = pomiń) */
WarpMaskBlit(
    src_bm,  sx, sy,        /* źródłowa bitmapa, pozycja */
    dst_bm,  dx, dy,        /* docelowa bitmapa, pozycja */
    w, h,                    /* rozmiar */
    0,                       /* kolor przezroczysty */
    0,                       /* flagi: 0 = sama przezroczystość */
    NULL);                   /* tablica cieni (nie używana) */

/* Kopia z przezroczystością + cień */
WarpMaskBlit(
    src_bm,  sx, sy,
    dst_bm,  dx, dy,
    w, h,
    0,                       /* przezroczysty */
    WARPF_SHADE,             /* włącz cień */
    shade_table);            /* 256 B: indeks → ciemniejszy indeks */
```

**Narzut:** ~1–2 µs na wywołanie biblioteki × 400 = 0,4–0,8 ms/klatkę. Akceptowalne.

#### Opcja B: lista poleceń w VRAM (najszybsza)

```c
/* Bufor poleceń w pamięci karty */
uint32_t *cmd = WarpGetCmdBuffer(screen);

/* Polecenie: 5 słów na sprite */
*cmd++ = WARP_CMD_MASKBLIT | WARPF_SHADE;
*cmd++ = src_offset;         /* offset w VRAM źródła */
*cmd++ = dst_offset;         /* offset w VRAM celu */
*cmd++ = (w << 16) | h;     /* rozmiar */
*cmd++ = (src_stride << 16) | dst_stride;
/* ... powtórz dla kolejnych sprite'ów ... */

/* Koniec listy: jedno polecenie uruchamia FPGA */
*cmd++ = WARP_CMD_END;
WarpExecCmds(screen);        /* zapis do rejestru → FPGA startuje */

/* CPU liczy logikę, FPGA rysuje sprite'y równolegle */
game_update();

/* Przed następną klatką: upewnij się, że FPGA skończył */
WarpWaitCmds(screen);
```

**Narzut:** ~0 (zapis do pamięci + jeden trigger). FPGA i CPU pracują równolegle.

---

## Pytania do ustalenia

**1. Zoom:** Czy scan doubler (cały ekran ×2) jest prostszy do zrobienia niż
skalowany blit (×2 przy kopiowaniu prostokąta)? W większości portów wystarczy zoom
całego ekranu. Settlers mógłby mieć GUI w natywnej rozdzielczości, ale może żyć
z powiększonym.

**2. CLUT + zoom razem:** Czy FPGA może jednocześnie czytać 8-bit z VRAM,
konwertować przez tablicę i podwajać piksele? To daje największy zysk emulatorom
(zero pracy CPU na obraz).

**3. Masked blit — źródło danych:** W Settlers sprite'y leżą w Fast RAM
(zakodowane jako przebiegi). Czy FPGA czyta tylko z VRAM, czy też z Fast RAM (DMA)?
Jeśli tylko VRAM — gra musi wgrać dane sprite'ów do VRAM (jednorazowo, ~200 KB),
co jest do zrobienia.

**4. Cień — odczyt piksela docelowego:** Tryb cienia wymaga read-modify-write
w VRAM. Czy FPGA ma dostęp do VRAM z wystarczającą przepustowością, żeby czytać
piksel docelowy przy każdym pikselu cienia? W Settlers to ~6 400–16 000 pikseli
na klatkę — niewiele.

**5. Interfejs:** Wolisz bibliotekę (`warp.library`, łatwy fallback do CPU), listę
poleceń w pamięci karty (szybsza, ale sztywny format), czy rozszerzenie drivera P96
(tagi)?

**6. Synchronizacja:** Przy liście poleceń — czy CPU może pisać następną listę,
podczas gdy FPGA wykonuje bieżącą (podwójne buforowanie)?
