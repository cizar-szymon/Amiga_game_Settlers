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

**Kandydat do wydania: 0.96** (decyzja użytkownika: zawsze najnowsza sprawdzona wersja; 0.96 z grą
LAN, sprawdzona przez użytkownika 27.10.2026; gałąź `lan-game` scalona z `amiga-port`, wersja LAN
zastępuje zwykłą), znacznik `v0.96-rc`, archiwum `releases/amiga/Settlers-0.96.lha`.
Wcześniejsi: 0.61, 0.56, 0.54, 0.53, 0.52, 0.48, 0.41 (`v0.xx-rc`).
Amiga 1 po wydaniu (27.10.2026): w `WorkSD:Settlers` tylko gra 0.96, opisy, `Settlers.ini`, `data`
i zapisy (`freeserf`); pliki testowe (skrypty `run-*`, sceny `scene*.save`, `simtest`, `lansim`,
`lantest`, `datacheck`, `loadtest*`, `mapgentest`, `modelist`, `Settlers-hg`) przeniesione do
`WorkSD:Settlers/test`. Skrypty z `tools/amiga` i `tools/measure.sh` zakładają je w głównej
szufladzie – przed pomiarami `tools/install1.sh` albo przeniesienie z powrotem.
Amiga 2 (27.10.2026): `Settlers` 0.96 z opisami (wcześniej 0.61 i `Settlers-LAN` 0.67); usunięte
`Settlers-LAN`, programy i skrypty testowe, sceny pomiarowe (`scene1–3.save`); zostały dane,
`Settlers.ini` i zapisy w `freeserf/saves`.

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
6. `viewport.*`: efekty mapy tylko z obszaru wokół środka widoku (`hear_width/height`).
7. `viewport.*`, `gfx.*`: szybsze rysowanie widoku (przesunięcia sprite'ów, tablice `static`, bez
   float, hash obrazków) i sonda `draw_probe`.
8. `viewport.*`, `map.*`, `video.h`, `gfx.h`: drogi i granice w kafelkach terenu, powiadomienia
   `on_paths_changed`, sprawdzanie kafelków (`verify_tiles`).
9. `viewport.*`: dźwięki mapy raz na fazę animacji obiektu (brzęczenie przy wysokim FPS).
10. `game.cc`: tempo gry klawiszami ×1…×3 co ×0,5.
11. `interface.*`, `game.h`: tempo gry w prawym górnym rogu.
12. `data-source-amiga.*`, `viewport.*`: tekstury i maski terenu i dróg dekodowane raz; liczniki
    kafelków i zmian (statystyki).
13. `popup.cc`, `game-init.cc`, `interface.*`: EXIT w grze do menu startowego, QUIT w menu z
    pytaniem.
14. `popup.cc`: napis EXIT małymi literami.
15. `game-init.*`, `map-generator.cc`: podgląd mapy po chwili spokoju, pomiar etapów generatora,
    `remove_islands` z listą.
16. `game-init.*`, `interface.cc`: podgląd mapy generowany, gdy minie czas (okno rysuje się na żądanie).
17. `gui.*`, `viewport.cc`: Invert scrolling w obu osiach, tylko mysz.
18. `viewport.*`: kafelki terenu zostają przy zmianie rozmiaru widoku (powiększenie).
19. `popup.*`, `interface.cc`: QUIT znów z oryginalnym napisem, przycisk i klawisz „I” z okienkiem
    klawiszy.
20. `keys-box.*`, `interface.*`, `popup.*`: szersze okno klawiszy, napis PAUSE.
21. `interface.*`, `popup.cc`: okno platformy (fabryka z portu), przycisk Amiga w Options.

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
- **Options** (Amiga, 0.102): bez „Fullscreen video” (gra ma zawsze własny ekran); Messages pod
  Volume, pod nim **Aspect** 4:3 / 16:9 (kliknięcie zmienia i zawsze zaczyna od najniższego trybu:
  400×300 albo 640×360), niżej Screen.
- **Options → Screen:** strzałki poprzedni/następny tryb wybranego Aspect: 4:3 – 400×300, 640×400,
  640×480, 800×600; 16:9 – 640×360, 960×540 (1280×720 w 0.99–0.105, potem usunięty),
  zmiana między klatkami (`amiga_screen_reopen`, ramka ekranu zmieniana w miejscu,
  `notify_resize`, ponowne `use_frame`), wybór w `Settlers.ini` (`[amiga] mode`).
- **Options → Advanced** (Amiga): Show FPS (licznik Freeserf, zielony na czarnym, zapisywany),
  ukryte Graphics/Sounds/Music/Language, okno 288×152.
- Obrazek `button-advanced.png` wbudowany (`tools/embed_images.py` → `port/gen/resource_images.h`,
  `port/sprite-file-amiga.cc`), bo port nie ma dekodera PNG.
- Ikona: `tools/make_icon.py` (z Vastar, dodane `--stack`), stos 1 000 000, Tool Types w nawiasach.
  Od 0.56 ikona to logo „The Settlers” z obrazu startowego (`data/START.png`) na czarnym tle z napisem
  RTG w prawym górnym rogu, 120×56, 64 kolory: `tools/make_icon_picture.py data/START.png pic.png`
  (logo 96×30, RTG literami 3×5), potem `make_icon.py --frame pic.png --crop 0 0 120 56 --colors 64`
  (opcje `--size` i `--colors` dodane).

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

## Obraz startowy (0.34)

Między otwarciem ekranu a pierwszą klatką gry (ok. 3 s: dźwięk, losowa mapa, interfejs) ekran
był szary – domyślny kolor 0 nowego ekranu. Teraz `VideoAmiga` zaraz po otwarciu ekranu pokazuje
`<DATA>/start.iff` (`amiga_show_picture()` w `amiga_screen.c`: IFF ILBM do 8 bitplanów, ByteRun1,
zmniejszony do ekranu najbliższym pikselem i wyśrodkowany; paleta obrazu zajmuje kolory 0..n-1
do pierwszej klatki). Bez pliku ekran jest czarny. Plik robi `tools/make_start.py` z PNG
(800×445, 128 kolorów, 86 KB). Obraz to grafika oryginału (`data/START.png` od użytkownika), więc
jak dane gry nie trafia do repozytorium ani do wydań. Uwaga: zrzuty AmiAgenta (`shot`) z ekranu
8-bit gry mają przekłamane kolory także bez obrazu startowego – prawdziwe kolory daje F9.

## Strefa słyszenia efektów (0.35)

Freeserf odtwarza logikę dźwięku oryginału (4 kanały, kolejka 4 efektów na 1/50 s, niższy numer
efektu ważniejszy), ale efekty wyzwala każdy rysowany budynek i tragarz – w 800×600 widać kilka razy
więcej mapy niż na ekranie 320×200 oryginału, więc efektów było dużo więcej i nakładały się. Łatka
`0006`: dźwięki obiektów mapy (28 miejsc w `viewport.cc`) oraz liczniki drzew i wody (ptaki, fale)
tylko z prostokąta `Viewport::hear_width × hear_height` wokół środka widoku, domyślnie 320×200.
Kliknięcia interfejsu bez zmian. Tool Type `SFXAREA=WxH|ALL`. Koszt: jedno porównanie na obiekt –
scena 43,3 kl./s (było 43,2), miksowanie 0,81 ms na bufor (było ~1,1 ms), 0 przerw.

## Scena z wieloma postaciami (0.36)

Zgłoszenie użytkownika: przy kilku budynkach i wielu postaciach 12–13 kl./s, choć animacje (woda,
chorągiewki) wyglądają tak samo płynnie – liczone są z licznika gry (100/s, faza co 80 ms), nie
z klatek, a pętla nadrabia do 5 kroków logiki na klatkę. Zapis F10 (takt 38886, 640×400) jako
`WorkSD:Settlers/scene2.save`, skrypty `run-fps2`, `run-scene2`, `run-verify2`.

Nowa sonda (`STATS=FULL`, linia `VIEW`, `Viewport::draw_probe`): czas części widoku mapy w ms/s.
Na starcie (10,7 kl./s z sondami): postacie 462, obiekty 249, drogi i granice 138, teren 10.
Samo rysowanie pikseli sprite'ów (IMAGE) 273 – reszta to kod Freeserf przed rysowaniem.

| Krok (łatka `0007`) | scene2 kl./s | Uwagi |
|---|---|---|
| 0.35 | 14,3 | |
| tablice w `draw_row_serf`, `draw_serf_row`, `serf_get_body`, `draw_active_serf`, `draw_burning_building` jako `static` (były kopiowane na stos przy każdym wywołaniu, do 2,3 KB na postać) | 15,3 | |
| `draw_sprite_relatively` (głowa postaci) pamięta przesunięcia tułowia – wcześniej **dekodowało tułów z danych Amigi w każdej klatce dla każdej postaci** | **29,9** | postacie 449 → 134 ms/s |
| pamięć obrazków: `unordered_map` z własnym hashem zamiast `std::map` | 30,4 | w granicach szumu |
| bez arytmetyki float (programowej) dla całych sprite'ów (`progress` = 1) | **31,9** | +5% |

Scena wzorcowa scene1 (640×480 z dźwiękiem): 43,3 → **50,0** (pułap). `VERIFY` 0 różnic w obu
scenach. Dalsze możliwości: obiekty (budynki, drzewa, flagi – teraz największa część), drogi
(maskowane sprite'y).

## Drogi i granice w kafelkach terenu (0.37, Amiga 1)

Pomiar bez sond, przez pomijanie grup (tymczasowa opcja, scena 2, 640×400, 31,8 ms na klatkę):
drogi i granice **9,3 ms** (rysowanie dróg 5,8, granic 2,0, pętla po polach 1,8), drzewa / kamienie /
pola 9,2 (w tym cienie drzew 3,8), budynki 4,3, flagi 1,9. Wyłączenie samego wnętrza
`VideoAmiga::draw_image` daje 50 kl./s: piksele sprite'ów w VRAM to ~15 ms na klatkę, w tym cienie
(odczyt VRAM) 3,5 ms. `RENDER=FAST` (składanie w Fast RAM): 22,7 kl./s – gorzej niż VRAM. Sonda
E-clock na każdy sprite przekłamuje (kilka µs na odczyt E-clock) – do takich pomiarów tylko
pomijanie.

Łatka `0008`: drogi i granice rysowane przy tworzeniu kafelka terenu
(`draw_paths_and_borders_area` z marginesem 2 wierszy i kolumny), `Map` powiadamia
(`on_paths_changed`) przy zmianie drogi lub właściciela pola (`set_owner`/`del_owner` tylko przy
faktycznej zmianie – pętla terytorium woła `del_owner` dla całego kwadratu), widok unieważnia kafelki
pola i sąsiadów. Na ekranie tylko budowana droga. `PATHS=SCREEN` – po staremu.

Sprawdzenie:
- zrzuty F9 sceny zatrzymanej (`FREEZE`) w obu trybach: 173 różne piksele – kolejność nakładania się
  odcinków na skrzyżowaniach (zależy od wiersza, od którego zaczyna się rysowanie; Freeserf zmienia ją
  też przy przewinięciu) i odcinki przy dolnej krawędzi, które Freeserf ucinał;
- `VERIFY` sprawdza też kafelki (`TILES`: co 50 klatek rysowane od nowa i porównane). Scena
  `sceneai.save` (scene1 z graczem 0 jako komputerem – buduje drogi na ekranie), 4 min: bez
  unieważniania **30** nieaktualnych kafelków, z unieważnianiem **0**;
- `simtest` mission 0: ta sama suma `8eabef60` (logika bez zmian).

| Scena (Amiga 1, Warp1260) | 0.36 | 0.37 |
|---|---|---|
| scene2 640×400 | 31,3 | **44,5** |
| scene1 640×480 z dźwiękiem | 46,2 | **50,1** (pułap) |

Teraz największe są obiekty (`VIEW OBJ` 577 ms/s z sondami): drzewa z cieniami, budynki, flagi.

## Brzęczące efekty przy wysokim FPS (0.38)

Zgłoszenie: „pierdzenie, jakby przesterowanie” przy uderzeniach młotkiem. Dźwięki postaci i budynków
uruchamia kod rysowania (`viewport.cc`), zwykle „w fazie A zawsze, w fazie B gdy nie gra” (np.
budowniczy `(t & 7) == 4`). Faza animacji trwa 8 taktów (80 ms): przy 14 kl./s (0.35) dźwięk startował
raz na fazę, przy 44 kl./s (0.37) 3–4 razy – każdy start ucina poprzedni. Łatka `0009`: ten sam obiekt
(postać, budynek – bit 23) uruchamia ten sam dźwięk najwyżej raz na 8 taktów (`sfx_started`),
niezależnie od liczby klatek. Licznik `MAP SOUNDS n/s` w `RAM:Settlers-stats.txt` (STATS).
Scena 2, 10 próbek po 1 s: bez ograniczenia 32 starty, z ograniczeniem 5; FPS bez zmian (44,4).

## Obraz startowy z muzyką (0.39)

Obraz startowy zostaje co najmniej 4 s od pokazania (`VideoAmiga::start_picture_ms`, czekanie
`amiga_wait` przed pętlą gry, Ctrl-C kończy). Muzyka startowała w `main` przed `apply_settings`, więc
przy wyłączonej grała ok. 1 s: teraz `main` ustawia ją z `Settlers.ini` (`options/music`) przed
`play_track`, a `PlayerMusic::play_track` wczytuje moduł także przy wyłączonej muzyce (bez grania),
żeby późniejsze włączenie (M, Options) działało.

## Tempo gry klawiszami (0.40)

Freeserf: `+` dodawał 1 takt na krok (do 40, czyli ×20), `-` schodził do 0 (pauza). Łatka `0010`:
tempo 2 (×1) do 6 (×3) co 1 (×0,5), `-` nie schodzi poniżej normalnego, w pauzie `+`/`-` zmieniają
tempo po pauzie (`game_speed_save`), `0` jak dawniej (normalne, kończy pauzę). Koszt CPU bez zmian
(50 kroków/s, każdy przesuwa zegar o więcej taktów). Pomiar na Amidze 1 (scena 2, klawisze przez
AmiAgenta, takty/s z czasem odczytu): ×1 96, ×1,5 151, ×3 304 (granica), `-` 258 / 194 / 101,
`0` 102; `-` przy ×1 bez zmian. Klawisze z klawiatury numerycznej i głównej działają tak samo.
0.41: tempo inne niż ×1 w prawym górnym rogu (`Interface::draw_game_speed`, `X1.5`…`X3`, styl jak
licznik FPS; przy ×1 i w pauzie nic) – sprawdzone zrzutami F9 przy ×1,5, ×2 i po `0`.

## 800×600 i kafelki terenu (0.42, Amiga 1)

800×600 (0.41, bez sond): scene1 37,8 kl./s (0.20: 32,5), scene2 31–32 stale, `STEPS 50`; scene2 z
`PATHS=SCREEN` 23,1 – drogi w kafelkach dają tu +38%. `STATS=FULL` przekłamuje teren (sondy) – do
pomiarów terenu tylko liczniki: `TILES DRAWN SO FAR n (ms)` w `RAM:Settlers-stats.txt` (narastająco),
`PATH CHANGES`, `HEIGHT CHANGES` (na sekundę). W zwykłej grze kafelki nie są rysowane od nowa.

Koszt kafelka 512×320 (tymczasowy pomiar w grze: ten sam kafelek 10 razy): zimny (pierwsze użycie
kombinacji tekstura+maska, 255 nowych obrazków) 1462 ms, z tego dekodowanie tekstury 772, obrazek
(RGBA → indeksy, odcinki) 360, nałożenie maski 160, maska 83; ciepły 71 ms, prawie cały w kopiowaniu
pikseli trójkątów: maski terenu Amigi są **ditherowane** (przejścia między rodzajami terenu), więc
na wiersz trójkąta przypada ~10 odcinków (177 tys. odcinków na 228 tys. pikseli kafelka).

Zmiany (łatka `0012` i port):
- `DataSourceAmiga`: tekstury i maski terenu i dróg dekodowane raz i zapamiętane (były dekodowane
  dla każdej kombinacji z maską);
- `VideoAmiga::create_image`: 32 ostatnie kolory obrazka bez tablicy haszującej;
- rysowanie do Fast RAM (kafelki, wzorzec `VERIFY`) obrazków bez cienia przez maskę bajtową
  długimi słowami (`masked_row`: cel = (cel & ~maska) | piksele), także przy przycięciu – zamiast
  odcinków i wolnej ścieżki piksel po pikselu. Kafelek identyczny piksel w piksel z rysowanym
  odcinkami (porównanie w pomiarze), `VERIFY` 0 / `TILES` 0 (wzorzec idzie teraz nową ścieżką).

| Kafelek | Przed | Po |
|---|---|---|
| zimny (pomiar) | 1462 ms | 621 ms |
| ciepły (pomiar) | 71 ms | 52 ms |
| start gry, 6 kafelków | 3048 ms | 1330–1390 ms |
| przewijanie, nowy kafelek | ~75 ms | ~58 ms |

FPS bez zmian (scene2 640×400 44,6, 800×600 32,0). Dalej: ciepły kafelek to przepustowość pamięci
(~600 różnych trójkątów); szarpnięcia przy przewijaniu usunęłoby dopiero rysowanie kafelków zawczasu.

## Exit do menu, Quit z menu (0.43–0.45)

Na życzenie użytkownika (łatka `0013`):
- okno ustawień (`sett_select`): przycisk QUIT (ikona 286) dostał napis EXIT – w danych gry takiego nie
  ma, więc rysowany: litery 5 px, żółte z ciemnym obrysem jak na sąsiednich przyciskach (0.46; w 0.45
  czcionką gry 8×8 – za duży); potwierdzenie „Do you want to quit this
  game?” → `Interface::exit_to_start_menu()`: przy następnym `update` zamknięcie okienka, nowa
  losowa gra i menu startowe jak przy starcie programu (zmiana gry z wnętrza okienka byłaby
  niebezpieczna – `set_game` kasuje widok);
- menu startowe: mały EXIT (ikona 60, `ActionClose`, chował menu) zastąpiony ikoną QUIT 32×16 (286)
  w tym samym rogu; pyta „Do you want to quit Settlers?” (`open_quit_program`), Yes zamyka program.
Sprawdzone zrzutami F9 (menu i okno ustawień); kliknięcia – użytkownik (AmiAgent nie ma myszy).
W oknie ustawień jest też mały EXIT z oryginału (zamyka okno) – dwa napisy EXIT, do decyzji.

## Podgląd mapy w menu startowym (0.47)

Zgłoszenie: losowanie mapy, zmiana typu gry (z LOAD dalej na Custom) i MAPSIZE > 6 trwają długo.
Każda z tych akcji generowała całą mapę (`ClassicMapGenerator`) dla podglądu. `port/mapgentest.cc`
(czas, etapy przez `mapgen_ms`/`mapgen_phase` w `map-generator.cc`, CRC mapy), Amiga 1:

| MAPSIZE | mapa | generowanie (0.46) | `-O2` (0.47) |
|---|---|---|---|
| 3 | 64×64 | 167 ms | |
| 6 | 256×128 | 1555 ms | 1296 ms |
| 7 | 256×256 | 3094 ms | |
| 8 | 512×256 | 6418 ms | 5326 ms |

Etapy (MAPSIZE 8, `-O1`): woda 1,5 s, pustynie 1,3 s, wysokości 1,2 s, wyspy 1,05 s, reszta < 0,35 s
każdy – czas liniowy z liczbą pól (~50 µs na pole, kilkadziesiąt przebiegów po mapie; sam przebieg
z 4 sąsiadami ~0,1 s). `remove_islands` przez listę zamiast powtarzanych skanów – bez zysku (mało
skanów), zostaje (ten sam wynik). `map-generator.cc` z `-O2` (bez wyjątków w pliku): −19%, CRC map
bez zmian (2df3a903, 1943881d), `simtest` 8eabef60.

Menu (łatki `0015`, `0016`): akcje tylko zamawiają podgląd (`request_map_preview`), mapa powstaje
0,4 s po ostatnim kliknięciu (0.48: okno menu rysuje się tylko na żądanie, więc termin sprawdza
`Interface::update` → `GameInitBox::update_preview`; w 0.47 napis zostawał na zawsze), do tego czasu w miejscu podglądu „Making the map” (minimapa ukryta); LOAD bez
generowania (podgląd zapisu). MAPSIZE 3→8 kliknięte szybko: jedna mapa (~5 s) zamiast pięciu (~12 s).
Dalej: start gry generuje mapę drugi raz (`GameInfo::instantiate`), wielkie mapy (9–10) nadal
kilkanaście sekund.

## Powiększenie mapy (0.49–0.50)

Prośba użytkownika: skokowe przybliżenie terenu ze środka ekranu w rozdzielczościach powyżej
400×300. Freeserf ma `Graphics::map_zoom`: widok mapy dostaje rozmiar ekran/zoom, rysuje się do
własnej ramki w Fast RAM i jest rozciągany (`draw_frame_scaled`); panel i okienka zostają; pozycja
myszy przeliczana w `GuiObject`. Port: `on_zoom` w `main` (środek ekranu zachowany:
`get_current_map_pos` → `move_to_map_pos`, interfejs znów rysuje w ramce ekranu – `use_frame`),
klawisz `Z` (przełącza), rolka myszy – RAWKEY 0x7A/0x7B (`RAWKEY_WHEEL_UP/DOWN`, OS 3.2 i NewMouse;
w górę ×2, w dół ×1, powtórzenia bez skutku); przy 400×300 powiększenie wraca do ×1.
`VideoAmiga::draw_frame_scaled`: szybka ścieżka ×2 (wiersz podwojony słowami 16-bit, dwa razy
`copy_row` do VRAM; wiersze z przezroczystością/cieniem po staremu).

Amiga 1, scene2 640×400: ×1 45,4 kl./s, ×2 **50,4** (pułap – rysowana ćwiartka mapy, 320×200),
z powrotem 45,5. Sprawdzone zrzutami F9; rolkę sprawdził użytkownik.

0.51: stopnie zależne od szerokości ekranu (`max_map_zoom`): ×2 powyżej 400×300, ×3 od 800×600
(widok ~267×200); rolka ±1 stopień, `Z` po kolei; szybka ścieżka skalowania dla ×2 i ×3. 800×600,
scene2: ×1 32,3, ×2 30,1 (widok 400×300 składany w Fast RAM bez blittera + zapis całego ekranu do
VRAM – wolniej niż ×1), ×3 43,0. Sprawdzone zrzutami (×1, ×2, ×3, ×3 bez zmian, rolka w dół ×2).

0.52: `STATS=FULL` pokazał, że to rozciąganie (`OTHER`) kosztuje 16–21 ms na klatkę (480 KB do VRAM);
`memcpy` zamiast pętli long – bez zmian (30,2). Na kartę idzie tylko to, co się zmieniło:
`scaled_prev` (poprzedni obraz widoku w Fast RAM) – wiersz bez zmian pominięty, zmieniony od pierwszej
do ostatniej różnicy; wiersze ekranu, na których rysowano coś innego (panel, okienka, kursor, napisy –
`mark_rows` w rysowaniu na ekran w VRAM) – w całości; bez obrazu z poprzedniej klatki – całość.
Maska bajtowa tylko dla obrazków o krótkich odcinkach (< 4 piksele średnio: trójkąty terenu), sprite'y
odcinkami (widok powiększony składa się w Fast RAM). 800×600 scene2: ×2 30,2 → **41,3**, ×3 44,5 →
**49,5**, ×1 32,5 bez zmian. `VERIFY` 0 przy ×2 i ×3, z przewijaniem, z okienkiem nad mapą i po
powrocie do ×1. Przy zmianie stopnia kafelki terenu są rysowane od nowa (6 kafelków, ~0,3 s).

## Gra LAN (gałąź `lan-game`, od 0.62)

Prośba użytkownika (26.10.2026): gra dwóch ludzi na dwóch Amigach w sieci lokalnej. Duży projekt,
więc prowadzimy go na gałęzi `lan-game` (Settlers i kopia Freeserf), a kandydat 0.61 (`main`,
`v0.61-rc`) zostaje bez zmian. Kompilacje z gałęzi idą na Amigę 2 jako `WorkSD:Settlers/Settlers-LAN`
(te same dane i zapisy, ikona skopiowana z `Settlers.info`).

Decyzje użytkownika: 2 ludzi + do 2 AI; mapę i AI ustawia serwer jak w New game; testy na Amidze 2
i WinUAE; punkt 1 to same okna, bez sieci.

| Punkt | Stan | Opis |
|---|---|---|
| 1. Okna | ✅ (0.62–0.66, ocenione przez użytkownika) | typ gry LAN w oknie startu, okno START SERVER / JOIN SERVER |
| 2. Połączenie | ✅ (0.67; z PC jako SLAVE sprawdzone w grze 26.10.2026) | `port/lan-amiga.cc` (bsdsocket): START SERVER nasłuchuje TCP 6810 i rozgłasza się UDP; dołączający znajduje go (SEARCH) albo wpisuje adres; powitanie z wersją; dołączający pojawia się w oknie serwera, serwer naciska START GAME |
| 3. Start wspólnej gry | ✅ (0.68; Amiga 2 + WinUAE sprawdzone w grze 26.10.2026) | SETUP (~100 B) z ziarnem liczb losowych (Freeserf brał je z zegara), obie strony budują grę same; dołączający gra graczem 2; 640×480; suma kontrolna co 5 s |
| 4. Lockstep | ✅ (0.69; Amiga 2 + WinUAE w grze, 26.10.2026) | polecenia graczy jako komunikaty w tym samym ticku, tempo z sieci, suma kontrolna co N ticków, rozłączenie |
| 5. Wydanie | ⏳ | 040 i 060, opisy, scalenie do `amiga-port` |

**Punkt 1 (0.62, łatka `0023`).**
- `GameInitBox`: nowy typ `GameLan`, wybierany jako trzeci: New game → Start mission → LAN game →
  Start tutorial → Load game (0.63, prośba użytkownika). Ikona New game (263) z napisem LAN u góry:
  0.63 kropki 2×2 (10 px), od 0.64 „minimalnie mniejszy” – litery 8 px z kreską 2 px, 22 px
  szerokości (`draw_small_letters` z dowolną liczbą wierszy i skalą, przeniesione z
  `PopupBox::draw_small_label` do `box-frame`, żeby nie dołączać grafiki z danych gry).
- Ustawienia jak New game na osobnej kopii `lan_mission`: gracze 1 i 2 to ludzie (twarze 12 i 13;
  w Freeserf twarz ≥ 12 to człowiek). Nie da się ich zmienić ani usunąć – tylko suwaki. Gracz 2 ma
  napis LAN. Gracze 3–4 dostają tylko twarze AI (1–11).
- Start otwiera `LanBox` (288×216, od 0.66 352×216 – opisy i adresy były za blisko; wartości od
  x 168, pola 132 px, lista 320 px; `Interface::open_lan_box`, Esc zamyka). Widoki:
  - wybór START SERVER / JOIN SERVER;
  - serwer (0.64–0.65, prośby użytkownika): nazwa serwera (pole; tę nazwę rozgłasza serwer
    i widzą ją szukający), adres serwera odczytany z Amigi, rozmiar mapy i liczba AI (liczba
    losowa usunięta – za długa), potem gracz 2 – nazwa i w osobnym wierszu adres (pojawią się po
    połączeniu; wtedy serwer startuje grę). Przyciski: **RUN SERVER** (po kliknięciu **STOP
    SERVER**; tylko uruchomiony serwer się rozgłasza), nieaktywny START GAME, CANCEL;
  - nazwy w `freeserf.ini` (`[lan] server_name`, domyślnie MASTER; `player_name` – „Your name”
    dołączającego, domyślnie SLAVE), zapisywane przy zamknięciu okna; litery, cyfry, spacja,
    `- _ .`, do 15 znaków;
  - dołączanie: lista serwerów, SEARCH, pole adresu z fokusem (cyfry i kropki, 15 znaków),
    CONNECT, BACK.
- Sieć jest za interfejsem `LanLink` (`src/lan-link.h`, Freeserf nie zna Amigi;
  `Interface::lan_link` ustawia `main`). Amiga: `port/lan-amiga.cc` + wywołania gniazd w C
  `port/amiga_net.c` (funkcje bsdsocket to makra – `connect`, `send` – kolidujące z C++).
  Już działa (0.65): adres tej Amigi (gniazdo UDP „połączone” z 192.0.2.1 i `getsockname`, bez
  wysyłania; zapasowo `gethostbyname(gethostname())`), RUN SERVER = nasłuch TCP 6810 + co sekundę
  rozgłoszenie UDP na 255.255.255.255:6810 `SETTLERS-LAN 1 <wersja> <nazwa>`. SEARCH i CONNECT
  dalej piszą „Network: the next step of the plan.”.
- Test z Shella: `lantest [n]` (adres, n rozgłoszeń, próba nasłuchu TCP 6810). Amiga 2: adres
  192.168.9.187, rozgłoszenia wysłane, nasłuch TCP działa. PC (192.168.8.126, ta sama podsieć /22,
  WiFi) nic nie odebrał (`tools/lan_listen.py`): profil sieci „Public” i reguły zapory blokujące
  Python – do testów z PC/WinUAE potrzebna reguła zapory na porty 6810 (decyzja użytkownika).

**Punkt 2 (0.67): protokół, proces sieciowy, połączenie.**

Decyzje użytkownika: sieć ma nie obciążać grafiki; gra LAN najwyżej 640×480 (przy 800×600
przełączenie na czas gry – punkt 3).

Protokół – lockstep z poleceniami, nie przesyłanie stanu:
- szukanie: UDP broadcast 255.255.255.255:6810 co 1 s, tylko przy RUN SERVER i dopóki nikt nie
  dołączył: `SETTLERS-LAN <protokół> <wersja> <nazwa>`;
- gra: TCP 6810 z `TCP_NODELAY`; ramka `długość (1 B: typ + dane) | typ | dane`. Proces obsługuje
  sam HELLO (protokół, wersja, nazwa; inna wersja → BYE i komunikat), BYE, PING/PONG co 1 s;
  ramki gry (typ ≥ 16) idą do gry;
- dane w grze (punkt 4): TURN co 4 kroki (80 ms) + polecenia 3–30 B, SUM co 50 kroków →
  ~100 B/s danych w każdą stronę, z nagłówkami TCP/IP ~0,7 KB/s;
- start gry bez przesyłania zapisu: SETUP (~60 B: mapa, gracze, ziarno `Game::rnd`), mapa
  generowana na obu Amigach, zgodność sprawdza SUM w kroku 0.

Proces sieciowy (`port/amiga_net.c`): „Settlers LAN”, priorytet 1, startuje przy pierwszym
użyciu, kończy się z grą (`LanLinkAmiga` w `main` przed `Interface`).
- Całe `bsdsocket.library` należy do procesu (gniazda są przypisane do zadania).
- Czeka w `WaitSelect` na gniazda, sygnał gry i Ctrl-C. Bez serwera, szukania i połączenia czeka
  bez limitu czasu (0% CPU), inaczej budzi się co 200 ms (rozgłoszenie, limity, PING).
- Gra ↔ proces: dwa pierścienie po 8 KB (jeden pisze, jeden czyta, bez blokad), gra budzi proces
  `Signal`. Gra nie wywołuje żadnej funkcji sieci (`LanLinkAmiga::poll` czyta tylko pierścień).
- Jeden gracz na serwer: następny dostaje BYE „busy”. Po zniknięciu gracza serwer czeka dalej
  i znowu się rozgłasza.

Okno LAN (0.67):
- MASTER: po dołączeniu wiersze Player 2 / Address / Response (czas PING) i aktywny START GAME
  (wysyła SETUP, u SLAVE „The server starts the game...”; sama gra to punkt 3);
- SLAVE: SEARCH (3 s nasłuchu; lista nazwa + adres, inna wersja szara z numerem; kliknięcie
  wpisuje adres), CONNECT (limit 5 s), widok „Connected” z Response i DISCONNECT;
- adres serwera i nazwy zapamiętywane w `freeserf.ini` (`[lan]`).

Testy (26.10.2026, Amiga 2 + PC, `port/lantest.c` z tym samym procesem co gra,
`tools/lan_peer.py` udający drugą stronę):
- serwer na Amidze, PC dołącza: HELLO w obie strony, PING/PONG, rozłączenie wykryte. Koszt
  procesu sieciowego: **34 ms w 25 s** (65 ramek w każdą stronę) – ~0,14% CPU 060;
- PC rozgłasza serwer, `lantest search` na Amidze go znajduje (na 255.255.255.255 i na
  192.168.11.255);
- w drugą stronę PC nic nie odbiera (zapora Windows, sieć „Public”) – do testów z WinUAE
  potrzebna reguła na port 6810;
- czas odpowiedzi do Amigi 2: PING z gry mediana ~100 ms, do 1 s; **zwykły `ping` z PC: średnio
  144 ms, do 696 ms** (bramka 1 ms). Opóźnienie robi łącze Amigi 2, nie protokół. Dla lockstepu
  (punkt 4): opóźnienie poleceń trzeba dobierać do zmierzonego czasu odpowiedzi, nie stałe 160 ms.
- `ping` z samej Amigi 2 (Roadshow `C:ping`, bez gry): do routera min 5 / średnio 157 / max 780 ms,
  do PC 6 / 29 / 126 ms (WiFi Amigi 2 słabe i zmienne); PC do routera 1 ms.
- Amiga 1 (A1200, Warp1260, `warpNET` kablem, przez VPN): do swojego routera 2,7 / 4,6 / 10 ms;
  PC → Amiga 1 (`ping`) 18 / 38 / 147 ms. **Protokół gry przez VPN** (`lantest serve` na Amidze 1,
  PC dołącza, 200 PING): **średnio 23,5 ms, mediana 22,6 ms, najgorszy 47,7 ms**; proces
  sieciowy 96 ms w 30 s przy 221 ramkach w każdą stronę (~0,4 ms na parę ramek, 0,3% CPU).
  Amiga 1 nie ma trasy do sieci PC i Amigi 2 (ping 100% strat) – VPN łączy tylko PC z siecią
  Amigi 1, więc gra Amiga 1 ↔ Amiga 2 przez VPN nie jest możliwa bez trasy w VPN.

**Punkt 3 (0.68): start wspólnej gry** (łatka `0024`).
- `src/lan-sync.cc`: SETUP jako tekst `1 <rozmiar mapy> <random base> <ziarno> <gracze>` + dla
  gracza `twarz r g b inteligencja zapasy rozmnażanie` (~100 B). Serwer też buduje grę z własnego
  tekstu SETUP, więc obie strony mają ten sam `GameInfo`.
- Ziarno liczb losowych gry: `GameInfo::set_game_random` → `Game::set_random` przed `Game::init`
  (Freeserf brał je z zegara). Nowe przy każdym START GAME.
- `Interface::start_lan_game(setup, server)`: `GameManager::start_game`, `LanGame` przejmuje
  połączenie (okno LAN go nie zamyka), u dołączającego `set_player(1)`, zamyka okna LAN i startu.
  Wyjście do menu kończy `LanGame` (połączenie, powrót trybu ekranu).
- `LanGame::started`: ekran większy niż 640×480 → 640×480 na czas gry (decyzja użytkownika);
  `main` nie zapisuje tego trybu do ustawień; po grze wraca poprzedni.
- Suma kontrolna `lan_checksum` (FNV-1a: krok, stan liczb losowych, kafelki mapy – wysokość,
  drogi, właściciel, obiekt, postać, surowce) w kroku 1 i co 250 kroków (5 s), wysyłana jako
  ramka gry 1 i porównywana z sumą drugiej strony z tego samego kroku. Napis w grze pod FPS:
  `LAN <nazwa> <ms> same <krok>` albo na czerwono `OUT OF STEP <krok>`; `player gone` po
  rozłączeniu.
- Bez poleceń graczy (punkt 4) obie gry liczą się dalej same i zostają zgodne, dopóki nikt nic nie
  zbuduje – AI gra tak samo po obu stronach. Pierwsze kliknięcie (np. zamek) je rozjedzie.
- **`lansim`** (port, bez sieci): gra z SETUP (2 ludzi + 2 AI, mapa 4), 3000 kroków. Na Amidze 2
  dwa uruchomienia: identyczne sumy LAN i CRC32 całego zapisu w krokach 1, 501 … 3000.
  Amiga 1 (Warp1260, 0.69): te same sumy co Amiga 2 i WinUAE – trzy maszyny zgodne.
- `lansim` w WinUAE (68060 bez JIT, `C:\amiga-dev\winuae\Settlers_A1200_OS32_net.uae`, wolumin
  `SETTLERS:` = `build/amiga` z kopią `data/`): te same sumy co Amiga 2 we wszystkich punktach.
- **Test w grze (26.10.2026):** MASTER na Amidze 2, SLAVE w WinUAE na PC (zapora Windows: reguły
  „Settlers LAN 6810 TCP/UDP”, tylko sieć lokalna i 192.168.5.0/24). SEARCH znalazł serwer,
  CONNECT, START GAME – obie gry wystartowały, sumy zgodne; po postawieniu zamku na Amidze napis
  `LAN SLAVE 21ms OUT OF STEP 1001` – rozjazd wykryty przy pierwszym porównaniu po kliknięciu,
  zgodnie z oczekiwaniem (polecenia graczy to punkt 4).
- `tools/lan_peer.py connect … --echo-sums` odsyła sumy serwera: test napisu w grze bez drugiej
  Amigi.

**Punkt 4 (0.69): lockstep** (łatka `0025`).
- `src/lan-command.cc`: `LanCommand` – wszystko, co gracz zmienia w grze: flaga, droga (z flagą na
  końcu albo bez), budynek, zamiana budynku, zamek, burzenie flagi i budynku, łączenie dróg przez
  flagę, geolog, tryby magazynu, szkolenie rycerzy, atak (rycerze liczeni od nowa przy wykonaniu,
  na obu Amigach), pauza, tempo i ustawienia graczy (suwaki towarów i narzędzi, obsada budynków
  wojskowych, rycerze w zamku, najsilniejsi/najsłabsi, resety, kolejność transportu flag
  i magazynów). Zapis: typ, gracz, pozycja, 3 liczby (18 B); droga + kierunki po 2 w bajcie.
- `Interface::command`: w grze na jednej Amidze wykonuje od razu (te same funkcje gry co dotąd),
  w grze LAN dodaje do kolejki `LanGame`. Wywołania w `interface.cc`, `popup.cc`, `viewport.cc`
  idą przez nie; w LAN interfejs sprawdza przed wysłaniem `can_build_*` (dźwięk „nie” od razu).
- `LanGame`: tury po 4 kroki (80 ms). Polecenia z tury t idą jako ramka TURN (numer tury, „więcej”,
  polecenia; dzielona, gdy nie mieści się w 250 B) i wykonują się na obu Amigach na początku tury
  t + opóźnienie: najpierw serwera (gracz 1), potem dołączającego. Opóźnienie wybiera serwer przy
  START GAME z czasu odpowiedzi: 1 + (ping + 40) / 80 tur, od 2 do 8 (21 ms → 2 tury = 160 ms);
  jest w SETUP (wersja 2). Bez TURN drugiej strony krok czeka (`Interface::update` wraca), po
  300 ms w linii LAN „WAITING”. Po rozłączeniu gra idzie dalej sama.
- Celowo inaczej w LAN: łączenie nowej drogi z istniejącą drogą specjalnym kliknięciem wyłączone
  (flaga powstałaby turę później) – od 0.96 jako jedno polecenie `TypeBuildRoad` z flagą na końcu
  (flaga na starej drodze, która ją dzieli, potem nowa droga – ta sama kolejność co bez LAN;
  sprawdzone w grze 27.10.2026: „działa”); koniec gry bez automatycznej pauzy (zależałaby od okien
  otwartych u jednego gracza). Lokalne zostają: wiadomości i timery (tylko komunikaty dla
  gracza), podgląd ataku (pola liczone przy wykonaniu od nowa).
- `lansim -l`: dwie gry w lockstepie (serwer i dołączający), każdy gracz stawia zamek, tartak,
  drogę i zmienia ustawienia; polecenia kodowane i dekodowane jak w sieci. WinUAE: 10/10 poleceń
  wykonanych w obu grach, sumy LAN i CRC32 całego zapisu identyczne w 13 punktach (3000 kroków).
  Amiga 1: te same wartości co WinUAE (krok 3000: LAN a5b07adf, zapis bfd8db93).
- **Test w grze (26.10.2026): Amiga 2 (MASTER) + WinUAE (SLAVE) – działa** (zamki i budowa po obu
  stronach, gry zgodne); Amiga 1 (MASTER, przez VPN) + WinUAE – działa, pauza po obu stronach.
- 0.70 (uwaga użytkownika: klient nie wiedział, że MASTER zamknął grę): wychodzący z gry (Exit,
  Quit, Ctrl-C) wysyła ramkę QUIT przed BYE. Dołączający po wyjściu serwera albo zerwaniu
  połączenia widzi na środku ekranu „MASTER ended the game.” / „The connection to … is lost.”,
  gra stoi 4 s i wraca do menu startowego. 0.71 (uwaga użytkownika: zapis gry jednej strony
  rozjechałby wspólną grę): tak samo u serwera – „SLAVE left the game.”, 4 s, menu startowe.
  Gra LAN kończy się zawsze po obu stronach.

**0.72: piasek na nowych mapach, tempo ×0,5** (prośby użytkownika; łatka `0027`; gra LAN działała
u użytkownika długo bez rozjazdu – „same” przy bardzo dużym numerze kroku).
- Options → Advanced: „Sand on new maps” 0–100% (kliknięcie: −10%, po 0% znowu 100%; `[advanced]
  sand` w `freeserf.ini`). `ClassicMapGenerator::set_desert_percent`: pustynia rysowana tylko
  z wewnętrznej części spirali (256 × % pól zamiast 256); liczby losowe brane jak bez zmiany, więc
  reszta mapy zostaje ta sama, a przy 100% mapa jest identyczna z oryginałem (`lansim -l`: te same
  sumy co w 0.69). Działa dla New game i LAN game (też podgląd mapy, odświeżany po zmianie), misje
  i tutoriale bez zmian. W grze LAN wartość serwera idzie w SETUP (wersja 3).
- `lansim -s PCT` liczy trójkąty pustyni; mapa 6: 100% 6808, 50% 3153, 25% 1106, 0% 0 z 65536.
- Tempo: klawisz − schodzi do ×0,5 (`game_speed` 1), + wraca; napis X0.5 w prawym górnym rogu.
- 0.73 (uwagi użytkownika): wiersz Sand z wartością i przyciskami − + (ikony głośności z okna
  Options, po 10%); podgląd mapy w oknie startu rysuje się od nowa dopiero po zamknięciu Options.
- 0.74 (prośba użytkownika: brązowe kamienie tak samo jak piasek): „Rocks on new maps” 0–100%
  (`[advanced] rocks`). Dotyczy skał z piaskowca (`ObjectSandstone0/1`) i małych głazów
  (`ObjectStone`/`ObjectSandstone3`), nie kamieni dla kamieniarza (`ObjectStone0–7`).
  `create_random_object_clusters(..., percent)`: z obiektów, które stawia oryginał, zostaje
  równomiernie percent% (licznik), liczby losowe brane jak bez zmiany; przy 100% mapa identyczna
  (`lansim -l`: te same sumy). SETUP wersja 4 (rocks). `lansim -r PCT`, mapa 6: 100% 245 głazów,
  50% 124, 0% 0. Okno Advanced na Amidze ma 7 wierszy (288×192).
- 0.75 (uwaga użytkownika: „100%” wyglądało jak „sam piasek”): wartości jako „Normal” (jak
  oryginał), „None”, „50% of normal”; gdy tekst nie mieści się między opisem a − +, „50% of
  norm.”, a w ostateczności „50%”. Okno Advanced na Amidze 312×192 (nadal mieści się w 320).

**Zapis i wczytanie gry LAN (0.76, łatka `0028`).**
- Zapis: każda strona zapisuje grę LAN zwykłym oknem Save (stan obu jest ten sam).
- Wczytanie: u MASTER w oknie serwera przycisk SAVED GAME → lista zapisów (`ListSavedFiles`), USE
  GAME sprawdza, że gracz 2 w zapisie jest człowiekiem (inaczej „Player 2 is not a person
  there.”). Wiersze pokazują nazwę zapisu i rozmiar mapy; NEW MAP wraca do nowej mapy.
- START GAME: MASTER wczytuje plik jako tekst, `GameManager::load_game_text` (jak `GameStore::read`,
  potem `speed_reset` – zapis nie trzyma tempa, obie strony startują z normalnym) i wysyła go
  w całości: SAVE BEGIN „rozmiar suma opóźnienie”, kawałki po 240 B (ramki gry 5), SAVE END. Gra
  MASTER nie robi kroku, dopóki wszystko nie wyjdzie (w linii LAN „SENDING THE GAME n%”), więc
  TURN-y idą po zapisie. SLAVE pokazuje „Receiving the game... n%”, sprawdza rozmiar i sumę
  FNV-1a i wczytuje ten sam tekst; gra jako gracz 2.
- 0.77 (uwaga użytkownika: zapis LAN wczytany zwykłym Load game grał tylko u MASTER): Load game
  w oknie startu z zapisem, w którym gracz 2 jest człowiekiem, otwiera okno LAN jako serwer z tym
  zapisem (`Interface::open_lan_box_with_save`); dalej RUN SERVER, dołączenie SLAVE, START GAME.
- 0.78 (zgłoszenie użytkownika: u SLAVE „The saved game came broken”): przy odbiorze proces
  wkładał ramki do pierścienia do gry szybciej, niż gra je brała (pierścień 8 KB, gra co 20 ms),
  nadmiar przepadał. Teraz ramka gry czeka w `in_buf` (4 KB), gdy w pierścieniu brak miejsca
  (`rx_blocked`, gniazdo nie jest wtedy czytane – dane czekają w TCP), a gra budzi proces
  (`net_event`, gdy pierścień w połowie pusty). Test: PC wysyła zapis 131 KB w 17 ms, `lantest
  connect` w WinUAE odbiera całość, suma zgodna.
- 0.79 (prośby użytkownika; wczytanie gry LAN działa): zapisy gry LAN mają w nazwie pliku `.lan`
  (`nazwa.lan.save`: okno Save, autosave, quicksave w grze LAN); lista pokazuje przed nazwą
  żółte LAN, a okno startu przy takim zapisie „Load LAN game”. Lista zapisów posortowana po
  nazwie; gdy zapisów jest więcej, niż się mieści (okno startu 18 wierszy, okno LAN i okno Save
  ok. 10), po prawej pasek z strzałkami i położeniem: kliknięcie w górną/dolną połowę – strona
  w górę/w dół (wcześniej tylko przeciąganie myszą, prawie niewidoczne). `get_selected` bez
  zaznaczenia nie sięga już poza listę.
- 0.80 (prośba użytkownika): Load game w oknie startu ma przycisk DELETE (nieaktywny bez
  zaznaczonego zapisu); po kliknięciu pytanie „Delete the save?” z YES / NO, YES usuwa plik
  (`std::remove`) i czyta listę od nowa. Przycisk tekstowy okien przeniesiony do `box-frame`
  (`draw_text_button`, wspólny dla okna LAN i okna startu).
- 0.81 (prośba użytkownika): podwójne kliknięcie zapisu wczytuje go – w Load game jak Start
  (przez `start_requested` w następnej aktualizacji, bo wczytanie zamyka okno razem z listą),
  w liście SAVED GAME okna LAN jak USE GAME (`ListSavedFiles::set_open_handler`).
- Proces sieciowy: ramka gry czeka w pierścieniu, póki nie zmieści się w buforze do gniazda
  (`ring_peek`; bufor 8 KB) – wcześniej nadmiar by przepadł.
- `lantest send <adres> <plik>` + odbiornik na PC: WinUAE → PC (127.0.0.1) 130 927 B w 0,3 s,
  suma zgodna. (Z WinUAE na 192.168.8.126 połączenie nie wyszło – zapora/trasa, 127.0.0.1 tak.)

**Pogoda – odrzucona.** Chmury i deszcz (gałąź `weather`, 0.81w1) użytkownik ocenił jako nietrafione;
gałąź zostaje lokalnie z jednym commitem, bez dalszej pracy.

**Więcej rodzajów drzew (0.82, łatka `0031`, prośba użytkownika: urozmaicenie bez kosztu
rysowania).** Options → Advanced „More kinds of trees” (`[advanced] tree_variety`, domyślnie
włączone). Z obrazów drzew z danych gry program raz robi warianty i trzyma je w pamięci obrazów
jak inne sprite'y (`Frame::draw_sprite_variant`, id z polem maski 0xf0+wariant), port rysuje je
tą samą szybką ścieżką z maską:
- odbite w poziomie (liściaste i iglaste; przesunięcie odbite wokół miejsca drzewa – pień na
  swoim miejscu; cień bez zmian);
- brzozy (tylko liściaste; także odbite). 0.82 przebarwiała drzewo gry – użytkownik: „nie
  wyglądają dobrze”. 0.83: własny pixel art `tools/make_birch.py` (wzór: zdjęcie brzozy od
  użytkownika `data/brzoza.bmp`, nie w repo; pomniejszone zdjęcie dawało chude, płaskie drzewo
  i nie jest nasze do rozpowszechniania): 32×30 jak drzewo gry, pień 2 px biały z cieniem i
  czarnymi plamami, owalna korona z kęp i prześwitów w zieleniach drzew gry, światło z lewej
  góry, 8 klatek wiatru (korona przechyla się o piksel). Pliki `port/resources/birch0–7.png`,
  wbudowane `tools/embed_images.py` (resource_images.h); rysowane z przesunięciem drzewa gry.
  0.84 (uwaga użytkownika: miganie): pień sięgał w głąb korony, a korona przesuwa się z wiatrem –
  przez jej prześwity raz było go widać, raz nie. Pień kończy się teraz pod koroną, bez gałązek.
Wariant wynika z pozycji na mapie (hash): liściaste 30% brzozy, 40% odbite; iglaste 40% odbite.
To tylko obraz – drzewo dla drwala to dalej to samo drzewo, w LAN obie strony widzą to samo.

**Okna Video i Audio (0.85, łatka `0032`, prośba użytkownika).** W oknie Advanced obok Exit (prawy
dolny róg) przyciski VIDEO i AUDIO. Otwierają w tym samym miejscu okna z tytułem u góry („Video”,
„Audio”; strony `AdvancedBox`, rozmiar wg strony, `Interface` centruje według `get_size`); ich Exit
wraca do Advanced. Video: Sand on new maps, Rocks on new maps, More kinds of trees (przeniesione
z Advanced, które ma znowu 5 wierszy, 312×152). Audio: na razie puste („Nothing here yet.”).

**Dziki i mniejsze napisy (0.86, łatka `0033`, prośby użytkownika).**
- VIDEO i AUDIO małymi literami 3×5 (jak napis EXIT), przyciski 40 px.
- Video → „Wild boars” (`[advanced] wild_boars`, domyślnie włączone): na wolnym polu, które
  wybiera hash pozycji (1/128 pól) i ma co najmniej 4 z 6 sąsiadów drzewami, stoi dzik – świnia
  z chlewni (`AssetGameObject` 0xa2–0xa8: chodzi trochę, je, odpoczywa), przebarwiona na ciemny
  brąz (`Frame::VariantDarkBrown`, raz, w pamięci obrazów). Co jakiś czas chrząka (dźwięk świni,
  tylko w strefie słyszenia). Tylko obraz i dźwięk – stan gry bez zmian, w LAN to samo u obu.
  0.87 (użytkownik: nie widać ich): co 24. wolne pole i co najmniej 3 drzewa wokół.
  0.88: głos dzika – nowy efekt `TypeSfxBoarGrunt` (92, wolne miejsce tabeli 96 efektów): próbka
  chrząknięcia świni z danych grana z okresem 580 zamiast 362 (ok. 0,62 wysokości – niżej,
  dłużej), głośniej. Nagranie z YouTube od użytkownika odrzucone: cudze, gra idzie na GitHub.
  0.89 (użytkownik: na Amidze nie słychać): efekt dostaje kanał tylko od dźwięku o tym samym lub
  wyższym numerze; odgłosy otoczenia (ptaki 70–82, fale 86, wiatr 88) grają długo i przy numerze
  92 zajmowały wszystkie 4 kanały. Teraz numer 61 (wolny, obok świni 60) i chrząkanie częściej
  (8/1024 na klatkę widocznego dzika w strefie słyszenia). Użytkownik: słychać.
  0.90 (prośba: jeszcze niżej): okres 760 (2,1× świni, ok. 0,48 wysokości), czas 101.
  0.94 (użytkownik: dzik został przy stawianym budynku): nie ma go, gdy przez jego pole idzie
  droga albo na sąsiednim polu jest budynek (także w budowie), flaga albo droga.

**Quit w oknie startu (0.91, łatka `0034`, zgłoszenie użytkownika: „często nie da się wybrać za
pierwszym razem”).** Przycisk rysowany w (308, 224) okna (`draw_box_icon(36, 208)` dodaje 16), a pole
kliknięcia `ActionClose` było w (308, 216) – reagowała tylko górna połowa przycisku i 8 px nad nim.
Teraz pole 308,224 32×16 we wszystkich czterech mapach kliknięć; zrzut WinUAE: przycisk w tym
miejscu.

**Koszt sumy kontrolnej LAN (0.92, łatka `0035`).** Pomiar na Amidze 1 (Warp1260, 68060), suma
całej mapy (`lansim -z N`, czas `lan_checksum`): mapa 4 – 15 ms, 6 – 58 ms, 8 – **231 ms**, 10 – ok.
0,9 s; liczona co 5 s, czyli wyraźne zacięcie na dużych mapach. Teraz: krok, stan liczb losowych
gry (gry, które się rozeszły, wołają je inaczej i szybko tu się różnią) i pas ok. 4096 pól mapy
– przy każdym porównaniu kolejny (numer porównania wybiera pas), skrót słowami 32-bit zamiast
bajtami. Czas: **3,2 ms na mapach 4, 6, 8 i 10**. `lansim -l` dalej zgodne (CRC całego zapisu jak
przedtem).

**Koniec gry LAN (0.93, łatka `0036`).** Przegląd: zwycięzcę wyznacza `Game::update_winner` (suma
punktów budynków ≥ 50, prowadzenie i w ziemi, i w wojsku) – liczone w logice, więc u obu w tym
samym kroku. Okno końca gry: obrazek wyniku, napis „THE <KOLOR> SETTLERS HAVE GAINED THE
SUPERIORITY”, potem pytanie o wyjście; w LAN bez pauzy (0.69), wyjście kończy grę u obu (0.71).
Błąd: obrazek wyniku był z punktu widzenia gracza 1 (zwycięstwo tylko przy `winner == 0`), więc
wygrywający SLAVE widział porażkę. Teraz w grze LAN: zwycięstwo, gdy zwycięzcą jest gracz tego
komputera, inaczej porażka. Typ gry zostaje `GameTypeOnePlayer` (Freeserf używa
`GameTypeTwoPlayers` tylko do obrazka w grze dwóch graczy na jednej Amidze).
Test (0.95, łatka `0038`): opcja `LANTEST` (Shell / Tool Type) – w grze LAN Ctrl+Shift+W wysyła
polecenie `TypeTestWin`: na obu Amigach w tej samej turze `Game::set_winner(gracz)` (zwycięzca
i koniec gry). Bez `LANTEST` klawisz nic nie robi.
Test w grze (27.10.2026, Amiga 1 + WinUAE, wygrana raz jednej, raz drugiej strony): obrazki
zwycięstwa i porażki po właściwych stronach, napis z kolorem zwycięzcy, wyjście kończy grę u obu –
użytkownik: „wszystko było ok”.

**Raspberry Pi 400 z Amiberry** (prośba użytkownika, 26.10.2026): ten sam program Amigi w emulatorze –
gra LAN Amiga ↔ Pi bez rozjazdów, jak z WinUAE. Pliki w `tools/pi/`: konfiguracja Amiberry
(68040 JIT, RTG, bsdsocket, wolumin WOW:), `install-amiberry.sh` (paczka `debian-<wersja>-arm64.zip`
z wydań Amiberry, 8.4.2), `pi_deploy.sh` (scp z PC), opis w `tools/pi/README.md`. Kartę SD
przygotowuje użytkownik (Raspberry Pi Imager, SSH); natywny Freeserf na Pi (SDL3) odłożony.


- logika bez liczb zmiennoprzecinkowych, bez wątków, kolekcje po indeksach, AI w
  `Game::update()`, test zgodności zapisów w `tests/test_save_game.cc` – podstawa lockstepu;
- zmiany stanu z GUI: `Game::build_*` / `demolish_*` / `send_geologist` / `set_inventory_*_mode` /
  `pull_roads_through_flag`, settery `Player` z okien ustawień, zapis wprost do
  `get_flag_prio()`/`get_inventory_prio()`, pola ataku + `start_attack`, `add_timer`, pauza i tempo;
- rysowanie płonącego budynku zapisuje stan budynku (`viewport.cc:1213`) – do sprawdzenia.

### Czat w grze LAN (0.97–0.98)

Tab otwiera i zamyka okno czatu (Esc też zamyka, wpisany tekst zostaje), Return wysyła. Wpis do
32 znaków czcionki gry (litery, cyfry, spacja, `- . : ? %`). Komunikat `MessageChat` (7): numer
gracza, nazwa, `\n`, tekst – idzie od razu, poza turami, bo nie zmienia gry; odbiorca czyści znaki i
długość. Okno `LanChatBox` (float interfejsu, lewy górny róg pod linią LAN): tło okna startowego
(ikony 290–294) i ramka okien (`draw_box_frame`, boki przycinane w niskim oknie), nazwa w kolorze
gracza (do 10 znaków), 8 ostatnich wpisów, linia pisania. Obraz okna rysowany tylko po zmianie
(klawisz, wpis, otwarcie); w klatce tylko kopia jak każdego okna. Zamknięte: wpisy widać 10 s po
nadejściu, z dźwiękiem wiadomości. Klawisze z Ctrl idą do gry; przy otwartym czacie litery i cyfry
nie. Sprawdzone przez użytkownika w grze Amiga 2 ↔ WinUAE (0.98).

## Ekrany panoramiczne 640×360, 960×540 i 1280×720 (0.99–0.100)

Wszystkie trzy są w 8 bitach na karcie Warp (`modelist`; jest też 1360×768). Options → Screen:
400×300, **640×360**, 640×400, 640×480, 800×600, **960×540** (0.100), **1280×720**; `MODE=` jak dotąd. **Zoom w 16:9 (0.105, decyzja
użytkownika):** ×2 i ×3 w 960×540 i 1280×720 jak w 4:3 (rozciąganie w tym samym trybie, nowy sposób
wysyłania z 0.104 – niżej), w 640×360 brak. W 0.103–0.104 zoomu w 16:9 nie było. Próby w 0.99–0.102: powiększenie ×2
procesorem w 1280×720 – 12,5 kl./s (wolniej niż bez niego); przełączanie trybu ekranu 1280×720 →
960×540 → 640×360 – szybkie (43–50 kl./s), ale monitor za każdym razem gaśnie (zamknięcie ekranu
pokazuje na chwilę Workbench, potem nowy tryb); `BitMapScale` karty – 437 ms na obraz (niżej). W 4:3
zoom bez zmian (rozciąganie w tym samym trybie, monitor się nie przestawia). Gra LAN (najwyżej
640×480): 1280×720 i 960×540 przechodzą na 640×360. Sprawdzone na Amidze 2 (`tools/widetest.sh`:
trzy razy `Z`, rolka w górę i w dół – tryb i FPS bez zmian). Wersje 0.100–0.103 (nie 1.00: 1.xx to
wydania oficjalne).

Pomiar (Amiga 2, Warp7060 100 MHz, zapis `aaazii`, `STATS`):

| ekran | kl./s |
|---|---|
| 640×360 | 43,7–50,1 |
| 960×540 | 23,8–27,0 |
| 1280×720 | 14,9–17,2 (1,9× pikseli 800×600: tam ~32) |
| 1280×720 → zmiana trybu na 640×360 (próba 0.99–0.102, odrzucona: monitor gaśnie) | 42,7 |
| 1280×720 z powiększeniem ×2 (próba, odrzucona) | 12,5; `STATS=FULL`: 8,8, rozciąganie `OTHER` 358 ms/s – ~40 ms na klatkę, prawie 900 KB do VRAM |

Skalowanie na karcie (`WarpBench/scalebench`, Amiga 2): `BitMapScale` 640×360 ×2 → 1280×720 437 ms,
480×270 ×2 → 960×540 246 ms – P96 na Warpie nie przyspiesza skalowania; blitter tylko 1:1 (cały
1280×720: 1,6 ms), zapis CPU całego 1280×720: 29,5 ms. Zoom bez przerwy obrazu tylko w FPGA (np. tryb
640×360 wysyłany jako 1280×720) – pytanie do autora oprogramowania RTG (Andrzej, Sellen).

### Powiększony widok: tylko zmienione kawałki wiersza (0.104)

Do karty szedł z każdego zmienionego wiersza widoku odcinek od pierwszej do ostatniej zmiany – dwie
postacie na końcach wiersza wysyłały wszystko między nimi. Teraz kawałki po 8 pikseli widoku, które
się zmieniły, łączone, gdy dzieli je mniej niż 16 pikseli (`draw_frame_scaled`, `zoom_gap`);
szybka ścieżka do szerokości 1296 (×3 widoku 427 pikseli = 1281 – wcześniej przy 1280×720 ×3 gra
spadała do pętli piksel po pikselu: 0,8 kl./s). Opcje testowe: `ZOOMGAP=n` (100000 = jak przed
0.104), `WIDEZOOM` (zoom także w 16:9 – tylko w 0.104, od 0.105 zawsze). Pomiar `tools/zoombench.sh`, Amiga 2, zapis `aaazii`,
kl./s (dwie próbki, druga):

| ekran | ×1 | ×2 stary / nowy | ×3 stary / nowy |
|---|---|---|---|
| 800×600 | 29,9 | 49,0 / 49,7 | 50,2 / 49,8 |
| 960×540 | 28,1 | 44,7 / **48,4** | 49,7 / 50,0 |
| 1280×720 | 17,3 | 12,0 / **18,4** | 37,4 / **44,2** |

`VERIFY` 0 (800×600 i 1280×720, ×1–×3). Ta scena jest lekka dla 800×600 (×2 już przy 50 kl./s) –
zysk widać przy dużym ekranie. 1280×720 ×2 (widok 640×360) zostaje na poziomie ×1: składanie widoku
w Fast RAM (teren bez blittera) i zapis ~900 KB przy zmianach w wielu wierszach.

0.105: nowy sposób jest jedynym (także w 4:3), zoom w 960×540 i 1280×720 bez opcji. Sprawdzone na
Amidze 2 (`tools/widetest.sh`): 640×360 – `Z` i rolka bez zmian; 960×540 – ×1 23,9, ×2 47,8, ×3 50,5;
1280×720 – ×1 14,9, ×2 18,1, ×3 44,6; rolka w górę ×2 (zrzut), w dół ×1.

**0.106: bez 1280×720 w Options** (decyzja użytkownika: 15–17 kl./s, na ciężkich mapach za wolno).
16:9 w Options: 640×360, 960×540. `Settlers.ini` z 1280×720 z wcześniejszych wersji otwiera 960×540
(sprawdzone na Amidze 2). `MODE=1280x720` w Shellu dalej działa do pomiarów (z zoomem).

## Koniec przeciągania to nie kliknięcie (0.107, zgłoszenie użytkownika)

Po przesunięciu mapy prawym przyciskiem znacznik skakał w inne miejsce, a gdy trafił na budynek,
otwierało się jego okno. Pętla zdarzeń (jak `event_loop-sdl.cc` Freeserf) po puszczeniu przycisku
wysyłała kliknięcie w miejscu początku przeciągania – mapa była już przesunięta, a kliknięcie prawym
to w porcie kliknięcie specjalne (znacznik + okno obiektu). Teraz `event_loop-amiga.cc`: gdy wskaźnik
w czasie przeciągania przeszedł więcej niż 8 pikseli (`drag_dist`), puszczenie nie jest kliknięciem
(ani podwójnym); mały ruch w czasie kliknięcia dalej klika w miejscu wciśnięcia. Sprawdzenie myszą –
użytkownik (AmiAgent nie przesuwa myszy): działa (Amiga 2, 0.107).

## Okno Amiga w Options (0.57–0.58)

Prośba użytkownika: przycisk „Amiga” obok Advanced z danymi komputera i zegarem Warpa. Łatka `0021`:
`Interface::platform_box_factory` (+ rozmiar) – Freeserf nie zna Amigi, port rejestruje okno w `main`;
przycisk w Options (`button-amiga.png`: duże A z przycisku Advanced i MIGA tymi samymi literami,
`tools/make_button_amiga.py`, od 0.59 tylko 38×16 – tyle co napis, wbudowany przez `embed_images.py`) tylko, gdy fabryka jest ustawiona.
Port: `amiga_sysinfo.c` – procesor z `AttnFlags`, dla 68060 typ i rewizja z rejestru PCR (`MOVEC PCR,D0`
przez `Supervisor`), FPU, wersja `exec`, `AvailMem` Chip/Fast, karta P96 (`P96BD_BoardName`,
`P96BD_TotalMemory`); jeśli jest `SYS:C/WarpTool`: `WarpTool Diag` do pliku w RAM: (model, firmware –
„3054” pokazywane jako 3.054 – zegar), zmiana `WarpTool CPUFreqHz` (od razu, wyjście do NIL:).
`amiga-box.cc` (288×216, jak Keys): wiersze danych, zegar ze strzałkami 220/221 – MC68060 50–100 MHz co
10 (podnoszenie powyżej zegara z chwili otwarcia tylko rev. 6, inaczej dźwięk „nie”), MC68040 20–50 MHz
co 1 (0.60, prośba użytkownika; rewizji 68040 się nie odczytuje), inne procesory bez strzałek;
ostrzeżenie o podkręcaniu. Okno sprawdził użytkownik na Amidze 2 (zrzut z wolnego łącza się nie pobrał).

### Wpisywanie nazwy zapisu (0.61, zgłoszenie użytkownika)

Przy nadawaniu nazwy zapisowi litery uruchamiały skróty: Z (zoom – port przechwytywał go w
`EventLoopAmiga::handle_key`, zanim zobaczyło go GUI), I, P, `+ - 0`, a cyfry 1–5 przyciski panelu
(pływające obiekty dostają klawisze w odwrotnej kolejności dodania). Łatka `0022`: `GuiObject::handle_event`
na najwyższym poziomie najpierw daje klawisz obiektowi z fokusem, jeśli on i jego rodzice są widoczni;
`ACTION_SHOW_SAVE` ustawia fokus na polu nazwy; `TextInput` obsługuje Backspace przed sprawdzeniem
długości (wcześniej przy pełnej nazwie nie dało się kasować) i oddaje grze klawisze sterujące (Esc
zamyka okno, Tab, Return) oraz znaki spoza 32–126. Z bez Ctrl idzie teraz przez `Interface::zoom_hook`
(ustawiany w `main` na `on_zoom`), kółko myszy dalej w pętli zdarzeń.

## Szybkość przy zegarze 50, 75 i 100 MHz (0.56, na prośbę użytkownika)

Amiga 1 (A1200, Warp1260X), zegar `WarpTool CPUFreqHz=<Hz>` – działa od razu, bez restartu (`Diag`
pokazuje, `simtest` potwierdza: 6000 kroków 5,2 s przy 100 MHz, 10,4 s przy 50). Pomiary zwykłe są przy
100 MHz; tych przy innym zegarze nie powtarzamy przy każdej poprawce, tylko na prośbę
(`tools/clockbench.sh MHZ OUT.csv`; po pomiarach zegar z powrotem na 100 MHz).

| kl./s | 50 MHz | 75 MHz | 100 MHz |
|---|---|---|---|
| logika: 6000 kroków / krok | 10,4 s / 1,73 ms | 7,0 s / 1,16 ms | 5,2 s / 0,87 ms |
| scena testowa 400×300 / 640×400 / 640×480 / 800×600 | 50,2 / 30,2 / 26,7 / 18,8 | 50,0 / 45,6 / 40,0 / 28,1 | 50,0 / 50,0 / 50,1 / 37,5 |
| gra z wieloma postaciami (scene2) 400×300 / 640×400 / 640×480 / 800×600 | 40,2 / 21,4 / 19,6 / 15,1 | 48,8 / 31,3 / 28,6 / 22,5 | 50,1 / 44,3 / 40,3 / 31,7 |
| scene2 800×600 ×2 / ×3 | 12,5 / 37,5 | 31,5 / 50,3 | 46,6 / 50,2 |

Dźwięk: 0 przerw we wszystkich. Przy 50 MHz powiększenie zmierzone dwa razy z tym samym wynikiem;
jeden przebieg dał 5–7 kl./s przy ×2 i ×3 i się nie powtórzył (przyczyna nieznana; liczniki kafelków w
powtórce bez zmian). Logika nawet przy 50 MHz < 2 ms na krok – zwalnia tylko obraz. Przy 50 MHz ×2 w
800×600 nie przyspiesza (składanie 400×300 w Fast RAM + rozciąganie CPU), ×3 tak. Raport PDF:
`docs/Settlers-raport.pdf`, rozdział „Szybkość przy zegarze”.

## Przycisk i klawisz Info (0.55)

Na życzenie użytkownika (łatka `0019`): na przycisku wyjścia do menu w oknie ustawień znów oryginalny
napis QUIT (działanie bez zmian); między SAVE a małym EXIT nowy przycisk 16×16 „I” (grafika małego
EXIT, litera 5 px jak na sąsiednich przyciskach – `draw_small_label`) → okienko `TypeKeysInfo` z
klawiszami (+, −, 0, Z, rolka, P, M, S, Ctrl+Z, F9, I, Shift+Esc); klawisz `I` otwiera to samo.
Czcionka gry nie ma „+” ani „/”: plus rysowany (`fill_rect` z cieniem), „on-off”. Sprawdzone
zrzutami F9 (okno ustawień, okienko Keys); kliknięcie „I” – użytkownik.

0.56 (łatka `0020`): okno klawiszy szersze – osobne okno `KeysBox` (272×160, ramka `draw_box_frame` jak
Advanced, dwie kolumny: klawisz, opis; mieści się w 400×300) zamiast okienka 144×160 (ramka okienek to
grafiki o stałym rozmiarze); przecinka też brak w czcionce. Pauza: duży napis PAUSE na środku
(`Interface::draw_pause`, litery 5×7 z kwadratów 6 px, żółte z cieniem), ukryty, gdy otwarte jest
okno. Sprawdzone zrzutami F9.

## Zmiana powiększenia bez rysowania terenu, testy zamykania (0.54)

`Viewport::layout` czyścił pamięć kafelków przy każdej zmianie rozmiaru widoku, więc każdy stopień
powiększenia rysował od nowa wszystkie kafelki w widoku (6 kafelków, ~0,3 s). Łatka `0018`: kafelki
zależą tylko od mapy – czyszczone tylko przy zmianie rozdzielczości ekranu (ich kopie na karcie
należą do starego ekranu). 800×600, rolka ×1→×2→×3→×2→×1, Z, Z: dochodzą tylko 1–2 kafelki wchodzące
pierwszy raz w kadr; `VERIFY` 0.

Testy zamykania (Amiga 1, 0.54; `tools/exittest.sh` poprawiony – okno szuflady „Settlers” na
Workbenchu nie jest już brane za grę, start przez `amiga_free.sh`): Ctrl-C w grze po 12 s ×5, w czasie
wczytywania (3 s) ×3, przy obrazie startowym (6 s) ×3, w menu (14 s) ×3, przy ×2 i ×3 po razie –
wszystkie bez błędu i bez pozostałego ekranu.

## Invert scrolling w obu osiach (0.53)

Freeserf odwracał tylko oś pionową przeciągania (`Viewport::handle_drag`, `ly = -ly`). Łatka `0017`:
obie osie, tylko dla myszy – `GuiObject::drag_button` (przycisk przeciągania); strzałki portu Amigi
przewijają przeciąganiem „przycisku 0” (`kKeyDrag`), więc zawsze w swoją stronę (wcześniej przy
włączonej opcji góra/dół były odwrócone). Sprawdził użytkownik (mysz).

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
