# Settlers (Freeserf) na Amigę RTG – plan i stan portu

Port **Freeserf** (C++, GPL-3.0, github.com/freeserf/freeserf), czyli otwartej wersji
The Settlers 1 (Blue Byte, 1993), na Amigę z kartą RTG i procesorem 68040/68060. Gra korzysta
z oryginalnych danych z wersji na Amigę. Ogólne zasady: `C:\Workprojects\AmigaGAMES\PORTING_PLAN.md`.

## Cel

- AmigaOS 3.2, Picasso96, **68060 i 68040** (Warp1260 w A1200, Warp4070 w A4000), Fast RAM.
- Własny ekran P96 **8-bit CLUT**, rozdzielczość z opcji: **640×480** (domyślnie) lub **640×400**; 800×600 do sprawdzenia pomiarem. Opcjonalnie okno na Workbenchu 16-bit.
- Dane z oryginalnej gry na Amigę (gfxheader, gfxfast, gfxchip, gfxpics, sounds, music).
- Pełne tempo logiki, **≥ 25 kl./s przy przewijaniu mapy** na Warp1260 / Warp7060 (mierzone na sprzęcie).
- 30 min gry z AI bez awarii, a zapis i odczyt gry działają.
- Dźwięk i muzyka przez Paulę, AHI jako opcja.

## Stan prac

| Faza | Stan | Weryfikacja | Uwagi |
|---|---|---|---|
| Plan, repo, decyzje | ✅ | ten plik | 25.10.2026 |
| F0 Rozpoznanie | ✅ | menu na PC z danymi Amigi; zależności opisane niżej | rozgrywkę sprawdza użytkownik |
| F1 Logika na 68k | ✅ | `simtest` na A1200: powtarzalne sumy kontrolne, 30 000 kroków, krok 0,6 ms | bez porównania z PC (decyzja użytkownika) |
| F2 Dane na 68k | ✅ | `datacheck` na A1200: 2322 sprite'y, dźwięki, muzyka; arkusze obejrzane | bez porównania z PC |
| F3 Backend Amigi | ✅ | menu, misja, drogi, kursor z gry, Tool Types i start z ikony, F9/F10 | 0.23–0.31 |
| F4 Wydajność | ✅ | cel fps, `VERIFY` bez różnic | 9,4 → 46,3 fps w scenie gry 640×480 (0.09 → 0.20), Amiga 2; Warp1260 równoważny (decyzja użytkownika) |
| F5 Dźwięk | ✅ | efekty i muzyka bez spadku fps poniżej celu | 0.22: Paula 22 kHz, efekty i muzyka potwierdzone odsłuchem; ~43 fps; AHI – opcja na później |
| F6 Wydanie | ✅ | czysta instalacja z archiwum `Settlers-0.32.lha` na Amidze 2 | 0.32 beta, 25.10.2026 |

## Decyzje

| # | Decyzja | Dlaczego |
|---|---|---|
| D1 | Sposób **D (port ze źródeł)**: fork `freeserf/freeserf` przypięty do commitu, nasze zmiany w osobnych plikach `*-amiga.cc` | jest kod źródłowy; łatwiej przenosić poprawki z upstreamu |
| D2 | Natywny backend: Intuition + P96 + `timer.device` + `audio.device`/AHI + `lowlevel.library`, bez SDL | na 68k nie ma SDL2/SDL3; pełna kontrola nad 8-bit i blitowaniem |
| D3 | Toolchain: bebbo amiga-gcc 6.5 w Dockerze (`docker/build.ps1`, obraz `sacredbanana/amiga-compiler:m68k-amigaos`, CMake `port/cmake/amiga-m68k.cmake`). Flagi `-mcpu=68020 -msoft-float -mtune=68060 -O1 -noixemul`, binarka sprawdzana `WarpBench/find64.sh` | vbcc nie kompiluje C++. Pułapki znane z AmiXcom: `-O2` psuje wyjątki C++, `-m68040` wybiera bibliotekę dla 68881. `-mtune=68060` usuwa 64-bit MULS.L (pułapka na 060). Wariant z FPU później, jeśli pomiar pokaże zysk |
| D4 | Dane: Amiga (`AmigaSettlersADF/`) jako główne, DOS `SPAx.PA` do porównań | użytkownik ma wersję na Amigę; DOS pomaga sprawdzać wyniki |
| D5 | Obraz: ekran P96 **8-bit CLUT**, `MODE=640x480` (domyślnie), `640x400`, ewentualnie `800x600` (decyzja po pomiarze F4), paleta z danych Amigi. Opcja `SCREEN=WINDOW`: okno na WB 16-bit, konwersja indeks → R5G6B5 przy wysyłce | dane mają 32 kolory; 8-bit to najmniej danych do VRAM; w 8/16-bit blit robi sprzęt Warpa |
| D6 | Renderer według `Warp RTG Developer notes.pdf`: składanie w Fast RAM (indeksy 8-bit), do VRAM tylko zapis zmienionych prostokątów (MOVE16 przy wyrównaniu do 16 B, inaczej `CopyMemQuick`), przewijanie przez `BltBitMap` w VRAM plus dosyłanie pasków. **Nigdy nie czytamy VRAM.** Korzystamy z doświadczeń AmiXcom (diff-copy, pomijanie niezmienionych klatek) | BltBitMap 514 MB/s, kopia przez CPU w VRAM 5,5 MB/s; odczyt po zapisie kosztuje 0,6 µs |
| D7 | Dźwięk: Paula (`audio.device`, bufory w Chip RAM). Opcje `SOUND=PAULA\|AHI`, `NOSOUND`. Format pliku `music` rozpoznamy w F5 | Paula najmniej obciąża CPU; AHI przez `warp.audio` jako opcja |
| D8 | **Bez procesora ARM** (rozszerzenia STM32 nie używamy) | decyzja użytkownika 25.10.2026; Warp1260/4070 i tak ich nie obsługują |
| D9 | Testy: build PC (SDL3) jako wzorzec, WinUAE (`*_net.uae` z woluminem `WOW:`), prawdziwe A1200 i A4000 przez AmiAgent, katalog `WorkSD:Settlers`. Pomiary wydajności tylko na sprzęcie | WinUAE zaniża koszty VRAM i CIA |

### Sterowanie

Gra jest obsługiwana myszą. Standard klawiszy:

| Klawisz | Działanie |
|---|---|
| strzałki | przewijanie mapy |
| **P** | pauza |
| Esc | wyjście (także Ctrl-C i zamknięcie okna) |
| F10 | zapis stanu do zgłoszeń błędów: zapis gry Freeserf do `RAM:Settlers-state.*` + `.txt` |

Z i X przypiszemy tylko wtedy, gdy znajdzie się dla nich sensowna akcja.

## Co wiemy o Freeserf (master, 25.10.2026)

- C++11, CMake ≥ 3.14. `master` używa **SDL3**.
- Biblioteki: `tools` i `game` (bez SDL), `data` (linkuje SDL3; warianty dummy dla
  SDL3_image/ttf i tłumaczeń: `sprite-file-dummy`, `font-dummy`, `translation-dummy`) i `platform`
  (`video-sdl`, `event_loop-sdl`, `audio-sdlmixer` / `audio-dummy`).
  Exe: `FreeSerf` oraz `profiler` (tylko `game` + `tools`, bez grafiki).
- `Video` (`src/video.h`): `create_image(void *data, w, h)`, `draw_image`, `draw_frame`,
  `draw_frame_scaled`, `fill_rect`, `draw_rect`, `draw_line`, `swap_buffers`, `set_cursor` i zoom
  (`float`). Kolory to RGBA 8:8:8:8, więc backend 8-bit musi zamieniać RGBA na indeks palety.
  Lepiej: wziąć indeksy wprost z `data-source-amiga` (do sprawdzenia w F0).

## Dane gry (lokalnie, poza repozytorium)

Trzy ADF-y (OFS) w `AmigaSettlersADF/`, wersja `[cr FLT]`. Wypakowane do `data/disk1..3`.
Sześć plików, których używa Freeserf, leży w `data/amiga`:

| Plik | Dysk | Bajty |
|---|---|---|
| gfxheader | 3 (Boot) | 96 |
| gfxfast | 3 | 171 816 |
| gfxchip | 3 | 211 322 |
| sounds | 3 | 112 010 |
| gfxpics | 2 | 114 256 |
| music | 2 | 243 474 |

Dysk 1 to intro. Na dysku 3 jest `TheSettlers` (192 688 B), czyli oryginalny program gry. W
`AmigaSettlersADF/SavedGames` są 44 zapisy oryginalnej gry: przydadzą się jako sceny testowe,
o ile Freeserf potrafi wczytać format z Amigi (do sprawdzenia w F0).

## Wyniki F0 (25.10.2026)

**Kod i build PC.** Freeserf sklonowany do `freeserf/` (gałąź `settlers-pin`), przypięty commit
`9b9270e1b2264071dc4b5fc9c398227abc30d8e9` (23.10.2026, wersja `0.5.9b9270e`). Build PC w MSYS2 UCRT64:

```
cmake -S freeserf -B build/pc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_SOUNDFONT=OFF
cmake --build build/pc --target FreeSerf profiler
build/pc/src/FreeSerf.exe -g data/amiga -r 640x480
```

Doinstalowane pakiety ucrt64: cmake, ninja, sdl3, sdl3-image, sdl3-mixer, sdl3-ttf.
Testy (`tests/`, googletest jako ExternalProject) nie budują się z Ninja: brakuje byproducts
`libgtest.a`. Do poprawienia w F1.

**Dane Amigi działają.** Menu z animowaną mapą wyświetla się poprawnie z `data/amiga`. Freeserf szuka
też pliku `extro` (dysk 3), skopiowanego do `data/amiga`. Okno ma 800×600 mimo `-r 640x480`
(prawdopodobnie skalowanie DPI Windows), do sprawdzenia.

**Zapisy oryginalnej gry z Amigi się nie wczytują** (`-l SettlerSave0`: „Invalid map size”). Tryb
zgodności obsługuje tylko format DOS. Sceny testowe zrobimy więc z zapisów Freeserf (Ctrl+Z
w grze) albo z mapy generowanej z ziarna.

**Pętla gry:** stały krok logiki `TICK_LENGTH` = 20 ms (**50 Hz, czyli tempo PAL**, jak oryginał
na wygaszaniu pionowym), maks. 5 kroków nadrabiania, potem rysowanie całej klatki. Nasz
`event_loop-amiga.cc` powtórzy to z `timer.device`. `profiler` wczytuje zapis i liczy
`game->update()` w nieskończoność. Do F1 trzeba własnego narzędzia: N kroków, potem suma
kontrolna stanu.

**Zależności poza `platform`:**
- SDL3 w `freeserf.cc` (`SDL_main`, `SDL_ShowSimpleMessageBox`) i `settings.cc` (`SDL_GetPrefPath`,
  `SDL_GetPathInfo`) – do zastąpienia wersjami dla Amigi (`PROGDIR:`, `Lock`);
- wyjątki C++ używane w całym kodzie gry (`throw` w game, savegame, serf, flag, building…) – bebbo gcc
  je obsługuje (AmiXcom też ich używa);
- brak `std::thread`, `<mutex>`, `<filesystem>`;
- 64-bit: `uint64_t` w AI (ścieżki dróg w `ai-road.cc` – przesunięcia/OR, tanie; mnożenie
  w `ai-manage.cc:601` → `__muldi3`) i klucze cache sprite'ów (`Data::Sprite::create_id`);
- `float`/`double`: w logice tylko `ai-manage.cc` i `ai-want.cc` (po jednym miejscu – sprawdzić
  w F1, czy wynik jest deterministyczny bez FPU); w GUI `viewport.cc` (postęp budowy, flagi
  zajęcia) i `gfx.cc` (konwersje kolorów CMYK). Zoom jest `float` – na Amidze zawsze 1.0.

**Grafika i 8-bit CLUT.** `data-source-amiga.cc` dekoduje bitplany przez tablice palet do RGBA.
Palety: `palette` (gra, 32), `palette2` (panel, 32), `palette3` (torsy osadników, różni się od
`palette` w indeksach 3 i 7), do tego `palette_intro`, `palette_logo`, `palette_extro`. W samej grze
jest **42 różnych kolorów**, do tego kolory 4 graczy i cienie: maska wypełniona
`{0,0,0,0x80}` (50% czerni). Wniosek: **stała paleta 256 kolorów wystarczy** – kolory gry, ich
wersje przyciemnione o połowę (jak EHB) i kolory graczy. Sprite'y 8-bit robimy, mapując RGBA na
indeks przy `create_image` (dokładne trafienia), a cień to tablica „indeks → indeks ciemniejszy”
zamiast mieszania alfa.

**Sposób rysowania (ważne dla F4).** `Viewport::draw_landscape` składa teren z kafelków
zapamiętanych w buforze (`landscape_tiles`, kopiowanych `draw_frame`). Na nie co klatkę rysowane
są wszystkie obiekty (drogi, granice, budynki, osadnicy, kursor) i panel. Cały ekran powstaje
od nowa w każdej klatce. Na Amidze trzeba więc: składać w Fast RAM, wysyłać do VRAM tylko zmiany
(diff jak w AmiXcom), a przy przewijaniu przesuwać obraz `BltBitMap`.

**Klawisze Freeserf a nasz standard:** `p` pauza (zgodnie ze standardem), strzałki – sprawdzić,
F10 w Freeserf to powrót do menu (konflikt ze standardowym F10 = zapis stanu: przenieść
powrót do menu na inny klawisz albo zostawić tylko w menu gry), Ctrl+Z zapis gry, `[`/`]` zoom
(na Amidze wyłączony).

**Muzyka** z danych Amigi to moduł **MOD** – Paula zagra go wprost (odtwarzacz MOD, np.
ptplayer), bez miksowania programowego.

## Fazy

### F0 – Rozpoznanie (S)
- Fork, przypięcie commitu, build PC (SDL3) w MSYS2 UCRT64, uruchomienie z danymi Amigi
  wypakowanymi z ADF.
- `grep -rn SDL src/` poza `platform`, szczególnie w `data`.
- Ustalić, jak `data-source-amiga` daje sprite'y (paleta → RGBA?) i gdzie w GUI jest `float` (zoom).
- **Weryfikacja:** gra działa na PC z danymi Amigi; zależności i interfejsy opisane tutaj.

### F1 – Logika gry na 68k bez grafiki (M) ⚠ największe ryzyko
- Plik toolchaina CMake dla amiga-gcc (`cmake/amiga-m68k.cmake`), budowa `tools`, `game` i `profiler`.
- Stos przez ikonę lub `Stack` (w AmiXcom `__stack` nie działał).
- Sprawdzić endianness (`freeserf_endian.h`) i zapis gry.
- **Weryfikacja:** ten sam seed daje tę samą sumę kontrolną stanu na A1200 (testów na PC już nie
  robimy, decyzja użytkownika 25.10.2026). Zmierzony czas ticku logiki i AI na 060.

#### Wyniki F1 (25.10.2026, A1200 Warp1260 68060)

- Budowa: `docker/build.ps1` → `build/amiga/simtest` (CMake `port/CMakeLists.txt` z plikami
  `tools` + `game` Freeserf, toolchain `port/cmake/amiga-m68k.cmake`). Kod gry skompilował się
  **bez żadnych zmian** w Freeserf. Jedyny konflikt nazw: klasa `Message` (Freeserf) a struktura
  `Message` (exec) – kod systemowy Amigi trzymamy w osobnych plikach C (`port/amiga_time.c`).
- `find64.sh`: 68 instrukcji 64-bit MULS.L/DIVS.L, **wszystkie w bibliotekach** (libnix
  `gmtime_r`/`strftime`, libgcc `__udivdi3`, iostream), żadnej w kodzie gry.
- `simtest` (`port/simtest.cc`): misja → N kroków `Game::update()` → CRC32 tekstowego zapisu stanu
  co K kroków. Freeserf inicjuje generator losowy gry zegarem (`Random::Random()` →
  `srand(time())`), więc `simtest` podmienia pole `random` w zapisie na stałe ziarno i wczytuje
  grę ponownie (opcja `-r`, `-r 0` = zegar). Gra wczytana z zapisu jest wstrzymana (prędkość nie
  jest zapisywana) – `speed_reset()`.
- Uruchomienie na Amidze: `Stack 1000000`, potem `WorkSD:Settlers/simtest -m 0 -n 30000 -k 5000`.
  Długie testy przez `start` z wyjściem do `RAM:` (klient AmiAgenta ma 30 s limitu na `run`).

| Misja | Kroki (czas gry) | Suma po 30 000 | Krok logiki śr. / maks. | Przebiegi |
|---|---|---|---|---|
| 0 | 30 000 (10 min) | `9a62fe31` | 0,63 / 13 ms | 2× identyczne we wszystkich punktach |
| 3 | 30 000 (10 min) | `715dff64` | 0,67 / 13 ms | 1× |

  Budżet kroku przy 50 Hz to 20 ms, więc logika z AI zajmuje ok. **3% czasu 68060 (Warp1260)**, a
  najdłuższy krok (13 ms) mieści się w jednej klatce. Po 10 min misji 0 gracz AI ma 7 budynków,
  57 osadników i 10 flag (gracz 0 to człowiek, stoi w miejscu).
- **Do optymalizacji:** wczytanie stanu z tekstowego zapisu trwa na 060 **4,6 s**
  (`GameStore::read`, iostream). Zapis gry przy CRC jest szybki. Dotyczy wczytywania gry i F10.
- Ryzyko, które zostaje: błąd zależny od kolejności bajtów, który na Amidze daje powtarzalny, ale
  inny niż na PC wynik, wyszedłby dopiero przy porównaniu z PC albo w grze.

### F2 – Dane na 68k (S)
- `data` z wariantami dummy, bez SDL.
- Program `datacheck`: wczytuje dane Amigi i DOS, liczy sumy sprite'ów.
- **Weryfikacja:** powtarzalne sumy na A1200 i obejrzane arkusze sprite'ów (bez PC).

#### Wyniki F2 (25.10.2026, A1200 Warp1260 68060)

- Biblioteka `fs_data` (`data`, `data-source-*`, `tpwm`, `sfx2wav`, `xmi2mid`, `pcm2wav` oraz
  `sprite-file-dummy`, `font-dummy`, `translation-dummy`) kompiluje się **bez zmian w Freeserf**
  i bez SDL.
- `datacheck` (`port/datacheck.cc`): `-g WorkSD:Settlers/data` (szuka podkatalogu `amiga`),
  dekoduje każdy zasób, liczy CRC32 i czas, z `-d KATALOG` zapisuje arkusz sprite'ów każdego
  zasobu do PPM. Sprite'y z maską (czcionka, flagi, torsy) biorą kolor z parametru `color` –
  test używa białego.
- Dane na Amidze: `WorkSD:Settlers/data/amiga/` (gfxheader, gfxfast, gfxchip, gfxpics, sounds,
  music, extro).
- Dwa przebiegi: identyczne sumy wszystkich zasobów (suma całkowita `f3eb018a`).
- Arkusze obejrzane: budynki, drzewa, kamienie, teren, ikony, portrety, ramki, czcionka, osadnicy –
  kolory i kształty poprawne, bez przesunięć bitplanów i zamiany bajtów.
- Brakujące indeksy (np. `sound` 39/90, `map_object` 137/194, `serf_torso` 469/541) to puste
  pozycje w danych Amigi: liczniki zasobów w `data.cc` są wspólne z DOS. Do sprawdzenia w F3, czy
  gra ich nie żąda.
- **Czasy na Warp1260 (68060):** wczytanie plików i rozpakowanie 2,7 s; zdekodowanie wszystkich zasobów
  8,1 s (najwięcej: muzyka 1,9 s, `art_box` 1,1 s, `serf_torso` 0,8 s, `map_object` 0,7 s,
  `icon` 0,6 s). Gra dekoduje sprite'y leniwie i trzyma je w pamięci podręcznej (`gfx.cc`), więc
  koszt rozłoży się na pierwsze użycia. W F3/F4: dekodować od razu do 8-bit (indeksy) i ewentualnie
  wstępnie przy starcie, żeby nie było przycięć w grze.

### F3 – Backend Amigi (M)
- `video-amiga.cc`: ekran P96 8-bit, `Frame` jako bufor indeksów w Fast RAM, wysyłka zmienionych
  prostokątów, kursor.
- `event_loop-amiga.cc`: IDCMP (mysz, klawiatura, zamknięcie), `timer.device`. Dźwięk na razie
  `audio-dummy`.
- Opcje w Shellu i jako Tool Types (`parse_option`), `STATS` w tytule okna, Ctrl-C (dla AmiAgenta).
- **Weryfikacja:** menu, start misji i budowa dróg na A1200. **Checkpoint A:** pomiar `STATS=FULL`.

#### Stan F3 (25.10.2026)

Pliki: `port/main-amiga.cc` (zamiast `freeserf.cc`), `port/settings-amiga.cc` (zamiast
`settings.cc`, `PROGDIR:Settlers.ini`), `port/video-amiga.cc/.h`, `port/event_loop-amiga.cc/.h`,
`port/amiga_screen.c/.h` (cały kod systemu Amigi, C), `port/settlers_version.h`,
`port/gen/src/version-vcs.h`. Program `Settlers` w `docker/build.ps1`; uruchamianie na Amidze
skryptem `WorkSD:Settlers/run-game` (`Stack 1000000`, `Settlers STATS`).

- **Wideo 8-bit:** każda ramka i obraz to indeksy palety; 0 = przezroczysty, 1 = cień (czarny
  z alfą 0x80 → przyciemnienie przez tablicę `shade[]`; cień na pustej ramce daje 1, więc działa
  także przy składaniu ramek). Paleta budowana w miarę pojawiania się kolorów, każdy kolor ma
  ciemniejszego bliźniaka. W grze 153–156 kolorów z 256.
- **Pętla:** krok 20 ms jak `event_loop-sdl.cc`, sen na `timer.device` + sygnał okna + Ctrl-C,
  timery (miganie panelu) obsługiwane w pętli. Przeciąganie myszą bez przenoszenia wskaźnika
  (SDL przenosi go z powrotem), kliknięcie po przeciągnięciu w miejscu początku – jak w SDL.
- **Klawisze:** strzałki przewijają, P pauza (gra), Esc zamyka okienka gry, **Shift+Esc / Ctrl+Q /
  Ctrl-C – wyjście** (Esc samo nie kończy gry, bo gra go używa – do decyzji użytkownika).
- **Opcje Shella:** `MODE=WxH` (domyślnie 640×480), `DATA=`, `LOAD=`, `STATS`, `DEBUG=`.
  Tool Types – jeszcze nie.
- **STATS** w tytule okna co 1 s: fps, ms/s logiki, składania, wysyłki, kroki/s, kolory.

Błędy znalezione na A1200:

| Wersja | Objaw | Przyczyna | Poprawka |
|---|---|---|---|
| 0.01 | AmiAgent zawieszony, śmieci w tytule ekranu | `SetWindowTitles(win, -1, buf)` ustawiało tytuł **ekranu** na bufor ze stosu | argumenty na miejscu, bufory statyczne |
| 0.02 | czarny pasek u góry miga | pasek tytułu ekranu przy zmianie tytułu okna | `ShowTitle(screen, FALSE)` |
| 0.03 | widać ćwiartkę planszy, brak panelu | `p96BestModeIDTags` dla 640×480 zwraca na Warpie tryb **320×200** | własny wybór trybu z `p96AllocModeListTags` (`port/modelist.c` wypisuje tryby) |
| 0.04 | niebieskie smugi przy granicy terenu i wodzie | **błąd kolejności bajtów** w `SpriteAmiga::make_transparent` (piksel jako `uint32_t & 0xFFFFFF`) – kolor przezroczysty (niebieski fal `0x0000BB`, czarny maski granic) nie znikał | porównanie bajtów B, G, R (łatka w Freeserf) |

Tryby 8-bit karty Warp (CLUT): 320×200/240/256, 400×300, 640×256/400/480/512, 800×600,
1024×768, 1280×720/1024, 1360×768.

**Pierwszy pomiar (0.04, menu z animowaną mapą, 640×480):** 7,1 fps; na sekundę: składanie
832 ms (~117 ms/klatkę), wysyłka `WriteChunkyPixels` 71 ms (~10 ms/klatkę), logika 95 ms;
tylko 35 kroków logiki/s zamiast 50 (limit 5 kroków nadrabiania na klatkę). To punkt wyjścia F4.

### Łatki w Freeserf (gałąź `settlers-pin` w `freeserf/`)

1. `savegame.cc`: na Amidze katalog zapisów `PROGDIR:freeserf` (zamiast `getenv("HOME")`).
2. `data-source-amiga.cc`: `make_transparent` porównuje bajty (błąd big endian).
3. `gui.h/.cc`: `GuiObject::use_frame()` – obiekt rysuje w podanej ramce (interfejs w ramce
   ekranu); dziecko w (0,0) o rozmiarze rodzica rysującego w miejscu też rysuje w miejscu (widok
   mapy), za to przy każdym przerysowaniu rodzica.
4. `popup.cc`, `advanced-box.*`, `interface.*`: Options → Screen (Amiga), Advanced → Show FPS,
   ukryte opcje danych i języka, mniejsze okno Advanced, licznik FPS zielony na czarnym.
5. `map.cc`, `savegame.*`: szybsze wczytywanie i zapis zapisów tekstowych (ten sam plik wyjściowy).

Wszystkie jako commity w `freeserf/` (gałąź `settlers-pin`) i pliki `port/freeserf-patches/`.

### F4 – Wydajność (M)
- Mierzymy na A1200 zawsze w tej samej scenie (z zapisu gry), według pętli z PORTING_PLAN.
- Kandydaci: przewijanie przez `BltBitMap` w VRAM, renderer przyrostowy / dirty rects, cache
  kafelków terenu mieszczący się w L2 (96 KB), brak wywołań systemu w pętlach, asm w blitach sprite'ów.
- Każda szybka ścieżka ma tryb porównania z wzorcową (`VERIFY`, `--check-render`).
- **Weryfikacja:** cel fps na A1200; wiersze w tabeli wydajności niżej.

#### Postęp F4 (25.10.2026, Amiga 2: A4000 Warp4070, Workbench zgłasza MC68060)

Amiga 1 niedostępna, pomiary na Amidze 2 (`C:/amiga-dev/amiagent-2.cfg`, ten sam układ
`WorkSD:Settlers`). Według użytkownika (projektanta kart) Warp7060 i Warp1260 mają w praktyce
tę samą wydajność (ten sam FPGA z grafiką, ten sam 68060), więc liczby z Amigi 2 obowiązują też dla
A1200 – osobny pomiar na A1200 nie jest potrzebny.

**Narzędzia pomiaru:**
- scena: `simtest -m 0 -c -n 6000 -s WorkSD:Settlers/scene1.save` (misja 0, zamek gracza 0
  przy środku mapy, 2 min gry); gra wczytuje ją z `LOAD=` i **od razu biegnie** – nie naciskać
  P (pauza; pierwsze pomiary 0.06–0.08 były przez to zrobione w pauzie i są nieważne);
- `STATS`: na ekranie tylko FPS, liczby co 1 s w `RAM:Settlers-stats.txt` (z `GAME TICK`, żeby
  widać było, że gra biegnie); `STATS=FULL`: sondy E-clock (kopie ramek / sprite'y / wypełnienia,
  ms i wywołania na sekundę) – spowalniają, tylko do pomiarów; `VERIFY`: każdy wiersz szybkiej
  ścieżki porównany z wzorcową, licznik różnic;
- F9: zrzut ekranu z prawdziwą paletą do `RAM:Settlers-shot.ppm` (zrzut AmiAgenta przekłamuje
  kolory ekranów 8-bit);
- skrypty: `tools/measure.sh` (wgranie i pomiar), `tools/getstats.sh`, `tools/exittest.sh`
  (wielokrotny start/Ctrl-C), `tools/reboot.sh` (`C:Reboot` przez AmiAgenta, zgoda
  użytkownika), `tools/addr2sym.py` (adresy z dziennika → funkcje).

**Podział czasu (0.09, biegnąca gra, sondy):** 9,4 fps; na klatkę: kopie ramek 40 ms (8 kopii:
kafelki terenu → widok mapy → interfejs → ekran), sprite'y 41 ms (~376 sprite'ów), wysyłka 10 ms,
logika 3 ms.

| Wersja | Zmiana | fps (scena gry) | Sprawdzenie |
|---|---|---|---|
| 0.09 | wyjście (pełne przerysowanie, kopie piksel po pikselu przez 3 ramki) | 9,4 (sondy) / 10,1 | – |
| 0.07 | kopiowanie po 4 piksele, gdy słowo nie ma przezroczystości ani cienia | (pomiar w pauzie) | `VERIFY` 0 |
| 0.08 | interfejs rysuje w ramce ekranu (`GuiObject::use_frame`) – jedna pełna kopia mniej | (pomiar w pauzie) | obraz F9 |
| 0.10 | widok mapy też rysuje w ramce ekranu (dziecko o rozmiarze rodzica pożycza jego ramkę) – kopie ramek 40 → 21 ms/klatkę | 11,4 | obraz F9 |
| 0.11 | sprite'y jako przebiegi (pomiń / kopiuj / cień), bez testu na piksel | **17,9** | `VERIFY` (wariant -hg: 15,3) |
| 0.12 | pusta `__chkabort()` – poprawka błędu przy wyjściu | – | `exittest` 6× bez błędu |
| 0.13 | kafelki bez przezroczystości kopiowane wprost (znacznik `changed`/`opaque` w ramce); wysyłka przez `p96LockBitMap` + MOVE16 (`BLIT=2`, domyślnie) | 22,7 (`BLIT=1`: 20,5, `BLIT=0` WriteChunkyPixels: 20,7) | `VERIFY` 0 |
| 0.14 | długie słowa przez typ `may_alias` zamiast `std::memcpy` (przy `-O1` gcc kopiował przez stos: dwa ruchy na słowo) | **24,3** | – |

Podział klatki w 0.14 (sondy, 20 fps): kopie kafelków 11 ms (~320 tys. pikseli, ~29 MB/s – blisko
granicy kopii Fast→Fast poza L2 według notatek Warpa), sprite'y 19 ms (~375 × ~50 µs), wysyłka 6 ms,
logika 1,5 ms.

**Składanie klatki w VRAM (`RENDER=VRAM`, domyślnie od 0.15; `RENDER=FAST` – stara droga):**
bufor klatki to bitmapa na karcie (przyjaciel ekranu). Kafelki terenu (512×320) mają kopie w VRAM,
wysyłane tylko po zmianie, i trafiają do bufora blitterem karty (`BltBitMap`). Sprite'y, panel i
okienka CPU pisze prosto w zablokowany bufor; na koniec jeden blit bufora na ekran. `VERIFY`
rysuje równolegle w Fast RAM i porównuje każdy wiersz bufora w VRAM (0 różnic).

| Wersja | Zmiana | fps | Uwagi |
|---|---|---|---|
| 0.15 | składanie w VRAM, blokada trzymana między wywołaniami | – | **zakleszczenie**: `LoadRGB32` przy założonej blokadzie, szary ekran, reset |
| 0.16 | blokada zdejmowana po każdym wywołaniu rysowania | 27,5 | teren 11 → 1 ms; wysyłka 6 → 0,6 ms; sprite'y 19 → 28 ms (bajty do VRAM, cienie czytane piksel po pikselu) |
| 0.17 | przebiegi sprite'ów do VRAM: kopiowanie wyrównanymi długimi słowami, cień – odczyt całego przebiegu, potem zapis | 32,7 | – |
| 0.18 | blokada przez całą klatkę (`LOCK=FRAME`; para lock/unlock kosztowała ~29 µs, ~8 ms na klatkę); `Log` do `RAM:Settlers-log.txt` | – | **zakleszczenie**: `AllocBitMap` kafelka przy założonej blokadzie |
| 0.19 | blokada zdejmowana przed każdym wywołaniem systemu na kafelkach | **36,3** | `VERIFY` 0, 2 blokady na klatkę, wyjścia bez błędu |
| 0.20 | cały sprite dekodowany w jednej funkcji (`draw_runs<kVram>`): bez wywołania na wiersz, kody w kolejności częstości, osobne wersje VRAM/Fast RAM | **46,3** | `VERIFY` 0 (nowa funkcja wiersz po wierszu wobec wzorca) |

Sondy sprite'ów (0.20, na klatkę): ~275 sprite'ów z przebiegów, ~12 przyciętych, ~86 poza ekranem;
~11 800 przebiegów, 45 tys. pikseli kopiowanych, 14 tys. cienia – koszt był w obsłudze przebiegów,
nie w pikselach. Pętla rysuje po każdym kroku logiki, więc 50 fps to górna granica.

Rozdzielczości (0.20, scena gry, Amiga 2): **640×400: 49,9 fps** (pułap), **640×480: 46,3 fps**,
**800×600: 32,5 fps**.

Zasada (z notatek Warp RTG, potwierdzona dwoma zakleszczeniami): **przy założonej blokadzie
bitmapy żadnych wywołań systemu, które rysują albo przydzielają na karcie** – paleta, blity,
`AllocBitMap`, tekst w oknie Shella. Przewijanie (krok 2) nie wymaga osobnej pracy: teren przy
dowolnym przesunięciu składa blitter w ~1 ms.

Podział klatki 0.19 (sondy, 28 fps): sprite'y ~21 ms (~375 sprite'ów), teren ~1 ms, pokazanie
~0,6 ms, logika ~1,3 ms, reszta GUI Freeserf ~4 ms. Dalej: szybsze sprite'y.

**Błąd przy wyjściu (znaleziony na Amidze 2):** „Corrupt memory list detected in FreeMem” /
„Freeing memory that is already free” po Ctrl-C, sporadycznie (powtarzalny `tools/exittest.sh`,
menu, co 3. próba). Wariant diagnostyczny `Settlers-hg` (strażnik sterty z AmiXcom:
`port/amiga_heapguard.c`, sprawdzanie co klatkę, dziennik `RAM:Settlers-heap.txt`) pokazał
podwójne zwolnienia w destruktorach statycznych (`~GameStore`, `~VideoAmiga`, lista uchwytów
`EventLoop`) – destruktory statyczne wykonały się dwa razy. Przyczyna: `read()`/`write()` libnix
wołają `__chkabort()`, które przy Ctrl-C robi `raise(SIGINT)` → `exit()` z dowolnego miejsca, także
w trakcie sprzątania. Poprawka (0.12): pusta `__chkabort()` w `amiga_screen.c`, Ctrl-C obsługuje
tylko pętla gry. Zawieszony proces po Ctrl-C miał tę samą przyczynę.

### F5 – Dźwięk (M)
- `audio-paula.cc`: efekty z `sounds`, muzyka z `music`. Opcja AHI.
- **Weryfikacja:** efekty i muzyka grają, fps nie spada poniżej celu.

#### Stan F5 (25.10.2026, Amiga 2)

- `port/amiga_audio.c`: proces „Settlers audio” (priorytet 10) dopełnia pierścień 4 buforów po
  512 próbek w Chip RAM dla jednego lewego i jednego prawego kanału `audio.device` (przydział
  kanałów jak w Vastar). Okres 161 na PAL = 22 030 Hz (zaokrąglony w górę, według planu).
  Wspólny stan miksera pod semaforem (`amiga_audio_lock`).
- `port/audio-amiga.cc`: `AudioAmiga` dla Freeserf – efekty to próbki Pauli (z WAV danych gry,
  16 bit → 8 bit) na 4 kanałach z głośnością i proporcją częstotliwości, które gra wylicza sama
  (`Audio::update_sfx`, jak oryginał); kanały 0/3 w lewo, 1/2 w prawo. Muzyka: moduł MOD z danych
  Amigi przez **micromod** (`port/thirdparty/micromod`, BSD-3, liczby całkowite), wzmocnienie 32
  z 64 (zapas na efekty). `NOSOUND` wyłącza dźwięk; klawisze gry `m` / `s` włączają muzykę/efekty.
- Koszt: ~1,1 ms miksowania na bufor 23 ms (~5% CPU), 46,3 → ~43 fps w scenie gry; 0 przerw
  (licznik `gaps` w `RAM:Settlers-stats.txt`).
- **Tempo muzyki:** moduł („siedler ii”, `M!K!`, 85 pozycji) ma na końcach wzorców `F2A`–`F3A`
  (wartości ≥ 32). micromod (jak ProTracker z CIA) czyta je jako BPM, muzyka spadała do 42–58 BPM
  na resztę utworu. Odtwarzacz gry działał w trybie wygaszania pionowego: każde `Fxx` to prędkość,
  tempo 125. W 0.22 micromod skompilowany z `MICROMOD_VBLANK` (opis w
  `port/thirdparty/micromod/README.port`). Narzędzie: `tools/modinfo.py` (komendy `Fxx`
  modułu); moduł zapisywany przy `STATS=FULL` do `RAM:Settlers-music0.mod`.
- Odsłuch użytkownika (0.22): efekty i proporcje dobre, tempo muzyki poprawne przez cały utwór.

#### Stan F3 – dokończenie (0.23–0.31)

- **Kursor z gry:** rysowany przez port przy każdej klatce (tło zapamiętane i odtworzone po
  pokazaniu), systemowy wskaźnik w oknie gry ukryty (`amiga_hide_pointer`), punkt aktywny 8,8.
- **Start z ikony:** `port/amiga_wb.c` – Tool Types przez ten sam parser co Shell (wpisy
  w nawiasach pomijane), komunikaty jako `EasyRequest`; dziennik `Log` do pliku przed ustawieniem
  poziomu (inaczej libnix otwierał okno konsoli na Workbenchu, a tekst w konsoli przy zablokowanej
  bitmapie karty grozi zawieszeniem).
- **F10:** `RAM:Settlers-state.save` (zapis Freeserf) + `.txt`; zapis trwa na 060 ~1–2 s.
- **Options → Screen:** strzałki poprzedni/następny tryb (400×300, 640×400, 640×480, 800×600),
  zmiana między klatkami (`amiga_screen_reopen`, ramka ekranu zmieniana w miejscu,
  `notify_resize`, ponowne `use_frame`), wybór w `Settlers.ini` (`[amiga] mode`).
- **Options → Advanced** (Amiga): Show FPS (licznik Freeserf, zielony na czarnym, zapisywany),
  ukryte Graphics/Sounds/Music/Language, okno 288×152.
- Obrazek `button-advanced.png` wbudowany (`tools/embed_images.py` → `port/gen/resource_images.h`,
  `port/sprite-file-amiga.cc`), bo port nie ma dekodera PNG.
- Ikona: `tools/make_icon.py` (z Vastar, dodane `--stack`), stos 1 000 000, Tool Types w nawiasach.

### F6 – Wydanie (S)
- Stała `SETTLERS_VERSION` (`0.xx` beta, `1.xx` oficjalne) widoczna w grze, w `$VER` i w tytule
  okna PC.
- Ikona (`make_icon.py`), Tool Types (`icon_tooltypes.py`), opisy PL/EN dla graczy i dla
  programistów, `Settlers_release.txt` / `_en.txt`, źródła (GPL-3.0). Bez danych gry.
- **Weryfikacja:** czysta instalacja z archiwum działa od zera. **Checkpoint B.**

#### Stan F6 (25.10.2026)

- Wydanie **0.32 beta**: `releases/amiga/Settlers-0.32.lha` (szuflada `Settlers`: program, ikona,
  opisy PL/EN dla graczy i programistów, plik wydań, `COPYING`, licencja micromod, pusta
  `data/amiga` z `ReadMe.txt`). Bez danych gry. Źródła opisów: `docs/release/amiga/`.
- Czysta instalacja na Amidze 2: rozpakowanie w `RAM:`, dane z dyskietek do `data/amiga`, start,
  wyjście bez błędu; menu ~50 fps w stanie ustalonym (pierwsze sekundy pierwszego startu wolniej –
  dekodowanie sprite'ów).
- Łatki Freeserf (gałąź `settlers-pin`) jako pliki: `port/freeserf-patches/0001..0004-*.patch`.
- Raport: `docs/Settlers-raport.pdf` (`tools/report_pdf.py`).

## Zawsze / Nigdy

- **Zawsze:** każda faza kończy się testem i wpisem w tabeli stanu;
  F1, F3, F4 i F6 testujemy też na prawdziwej Amidze.
- **Nigdy:** nie dystrybuujemy danych gry; nie zmieniamy reguł gry ani AI (poza optymalizacją
  z identycznym wynikiem); nie robimy wersji AGA; nie używamy procesora ARM; nie czytamy VRAM
  w pętlach rysujących.

## Wczytywanie i zapis gry (0.33)

Zapis tekstowy Freeserf (126 KB, scena 1; Amiga 2). Narzędzie `loadtest` (`port/loadtest.cc`):
czasy etapów i **CRC zapisu po wczytaniu** – musi zostać ten sam (`8eabef60` scena 1,
`8f9c3d9f` scena 3 z 10 min gry), co sprawdza i odczyt, i zapis.

| Krok | Wczytanie | Zapis |
|---|---|---|
| wyjście (libnix) | 2,22 s | 3,34 s |
| alokator AmiXcom (`amiga_fastmalloc.c`) sam | 1,73 s | 3,27 s |
| mapa: wartości sekcji raz zamiast 10 wyszukań nazwy na pole; listy bez `istringstream`; liczby zapisywane bez `ostringstream` | 1,60 s | **0,52 s** |
| liczby czytane w 32 bitach zamiast `strtoull` (64 bity w programie na 68k) | 1,03 s | 0,52 s |
| kawałki list budowane w miejscu (bez `substr`) | 1,03 s | 0,52 s |
| + alokator AmiXcom w grze (0.33) | **0,57 s** | **0,47 s** |

Rozkład wczytania przed zmianą mapy (sondy, z alokatorem): rozbiór tekstu i podział 408 ms, mapa
549 ms, obiekty 125 ms. Sprawdzanie spójności: 0 ms. Łatka `0005` we Freeserf; alokator w grze bez
wpływu na fps (43,2 w scenie z dźwiękiem), `VERIFY` 0, 4 wyjścia bez błędu.

## Wydajność (Warp1260 i Warp7060 – wydajnościowo równoważne)

| Data | Zmiana | Scena | fps / czas | Uwagi |
|---|---|---|---|---|
| 25.10.2026 | F4 0.20 (Amiga 2): sprite'y jedną funkcją | scena gry 640×480 / 640×400 / 800×600 | **46,3 / 49,9 / 32,5 fps** | `VERIFY` 0 |
| 25.10.2026 | F4 0.19 (Amiga 2): składanie w VRAM, teren blitterem, blokada na klatkę | scena gry, 640×480 | **36,3 fps** | `VERIFY` 0 |
| 25.10.2026 | F4 0.11 (Amiga 2): ramki w miejscu + sprite'y przebiegami | scena gry, 640×480 | 17,9 fps (z 9,4) | `VERIFY` 0 |
| 25.10.2026 | F3: pierwsza wersja (0.04), pełne przerysowanie + `WriteChunkyPixels` | menu z mapą, 640×480 | 7,1 fps; składanie 117 ms, wysyłka 10 ms na klatkę | logika 35 kroków/s |
| 25.10.2026 | F2: dane Amigi | `datacheck`, wszystkie zasoby | wczytanie 2,7 s, dekodowanie 8,1 s | RGBA, bez cache |
| 25.10.2026 | F1: sama logika, `-O1 -mtune=68060` | `simtest` misja 0, 30 000 kroków | krok 0,63 ms śr., 13 ms maks.; wczytanie stanu 4,6 s | bez grafiki |

## Znane różnice

- **Brak gry dwóch graczy.** Oryginał na Amigę miał podzielony ekran i dwie myszy; Freeserf tego
  nie przeniósł (został tylko `Game::GameTypeTwoPlayers`). Plan: gra sieciowa w trybie lockstep
  (polecenia graczy przez TCP, deterministyczna logika) – osobny etap po 0.32.
- **Rozdzielczości:** najniższa to 400×300 – okna gry są z oryginału na ekran PAL z overscanem
  (okno startowe 360×254, panel 352 szerokości), 320×200 ich nie mieści.
- **Esc** zamyka okienka gry (jak we Freeserf); wyjście z gry to Shift+Esc / Ctrl+Q / Ctrl-C
  (odstępstwo od standardu klawiszy portów, gdzie Esc kończy grę).
- **Freeserf F10** (nowa gra) zastąpiony standardowym F10 (zapis stanu); menu nowej gry: Ctrl+N.
- Bez tłumaczeń i bez czcionki `settlers.ttf` – teksty czcionką z danych gry, po angielsku.
- Liczby wydajności z Amigi 2 (Warp7060); Warp1260 w A1200 jest wydajnościowo równoważny.
