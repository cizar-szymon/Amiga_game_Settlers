# Zgodność zasad gry z oryginałem (Amiga) – porównanie z Freeserf

16.11.2026. Porównanie procedur oryginalnego programu gry z Amigi z kodem Freeserf, na którym
stoi port. Freeserf powstał z disasemblacji wersji DOS; tu sprawdzana jest wersja Amigi.

## Podsumowanie

Sprawdzone i **zgodne z oryginałem Amigi**: młyn, piekarnia, chlewnia, rzeźnia, huty, zbrojownia,
narzędziownia, stocznia, tartak, kamieniarz, drwal, leśniczy, rolnik, kopalnie (poza jedzeniem),
złoto w strażnicach, morale, place budowy, domyślne priorytety, szkolenie rycerzy, walka, zamiana
osadników w rycerzy, geolog, tablice i decyzje transportu, magazyny, obsada budynków wojskowych
(tablice chata / wieża / twierdza).

**Różnice** (szczegóły niżej):

| Nr | Co | Oryginał Amigi | Port (Freeserf) | Skutek |
|---|---|---|---|---|
| 1 | jedzenie w kopalniach | porcja w 1 wejściu górnika na 8 | porcja w 7 na 8 | w porcie kopalnie jedzą ~7 razy więcej (bogate złoże: 8 urobku z porcji w oryginale, 1,14 w porcie) |
| 2 | połów ryb | `los < ryby + 4` | `los + 4 < ryby` | rybak łowi średnio w 63% wyjść w oryginale, w 10% w porcie; woda z 1–4 rybami w porcie nic nie daje |
| 3 | przyciski szkól 1 / 5 / 20 / 100 (okno rycerzy, `docs/OKNO_RYCERZY.md`) | najwyżej tylu, ile na przycisku | wszyscy możliwi osadnicy | w porcie każdy przycisk zużywa wszystkie miecze i tarcze naraz |
| 4 | obrońcy zamku, gdy jest ich tylu, ilu chciano | najsilniejszy obrońca zamienia się z najsłabszym rycerzem czekającym w magazynie zamku – w zamku zostają najsłabsi | zamienia tylko poziomy dwóch obrońców między sobą, nic z magazynem | w porcie w zamku zostają rycerze, którzy przyszli (zwykle najsilniejsi), w oryginale silni wracają do magazynu i idą do budynków |

Drobne, bez wpływu na rozgrywkę: wybór sosna / drzewo liściaste u leśniczego (oryginał z pozycji
promienia ekranu, port z generatora gry), zaokrąglenia przy wyborze narzędzia i przy morale.

Opisy okien z numerami przycisków: `docs/OKNO_RYCERZY.md`, `docs/OKNO_OBSADY.md`, `docs/OKNO_OSADNIKOW.md`, `docs/OKNO_MAGAZYNU.md`.

## Metoda

- Program: `TheSettlers` z dyskietki 3 (`AmigaSettlersADF/…(Disk 3 of 3)(Boot).adf`,
  192 688 B, hunk, niespakowany; obraz `[cr FLT]` – crack nie zmienia logiki gry).
  Wyciągnięty `tools/adf_extract.py`, disasemblacja `m68k-amigaos-objdump -D -b amiga -m m68k`
  (obraz Dockera z `docker/build.ps1`). Adresy niżej: przesunięcie w sekcji `.text`.
- **Tablica stanów osadnika pod 0xBFF4**: skok przez `jmp %pc@(…,d0:w)` po bajcie stanu,
  wpis N = `Serf::State` N z `freeserf/src/serf.h` (np. wpis 29 → górnik 0xDF56). Każda procedura
  produkcji porównana instrukcja po instrukcji z `Serf::handle_serf_*_state` (`serf.cc`).
- Generator losowy oryginału (0x143FC: trzy słowa, dodawanie, xor, obroty) = `Game::random_int()`.
- Czasy w tikach gry: przy normalnym tempie 100 tików/s (`TICK_LENGTH` 20 ms, `game_speed` 2).

## Wyniki

| Co | Oryginał (Amiga, adres) | Freeserf / port | Zgodne? |
|---|---|---|---|
| **Kopalnie – jedzenie** | 0xDF56: w **1 wejściu na 8** górnik bierze porcję (`(los&7)==0` → stan 1), w 7 na 8 kopie bez jedzenia; gdy w wejściu „z jedzeniem” zapas pusty – czeka (co 256 tików) | odwrotnie: porcja w 7 wejściach na 8, 1 na 8 bez (`serf.cc` `handle_serf_mining_state`, stan 0) | **❌ różnica** |
| Kopalnie – kopanie | do 4 prób po 1000 tików, pole `(los>>2)&31` spirali, właściwe złoże, bez obiektu lub obiekt > zamek; odpoczynek 100 + (los&511); licznik „kopalnia pusta” | to samo | ✅ |
| Kopalnie – zapas jedzenia | do 8 (zamówione + leżące), priorytet = priorytet gracza (bajt) >> zapas | to samo | ✅ |
| **Rybak** | 0xDCEE: połów, gdy `(los&63) < ryby+4` | połów, gdy `(los&63)+4 < ryby` | **❌ różnica** |
| Rybak – reszta | do 10 prób na wyjście, na zmianę 128 / 768 tików, woda po lewej/prawej | to samo | ✅ |
| Młyn | 0xDA1A: 1 zboże → 1 mąka, animacja 137 (383), 2 × 1500 tików | to samo | ✅ |
| Piekarnia | 0xD95A: 1 mąka → 1 chleb, animacja 138 (767), 1500 tików | to samo | ✅ |
| Chlewnia | 0xD806: 1 zboże na cykl; rozmnażanie wg tablicy 6000, 8000, 10000, 11000, 12000, 13000, 14000 (świnie 1–7); świnia do rzeźni przy 8 albo > 3 i `20·los>>16 < świnie`; dalej bez zboża, gdy `los&15 ≠ 0` | to samo | ✅ |
| Rzeźnia | 0xD78A: 1 świnia → 1 mięso, 1535 tików | to samo | ✅ |
| Huty żelaza i złota | 0xDE7A: 1 ruda + 1 węgiel → 1 stal / 1 sztabka, 383 + 20 × 384 tików | to samo | ✅ |
| Zbrojownia | 0xD6B6: 1 węgiel + 1 stal → miecz, potem tarcza bez surowca; 575 + 6 × 576 | to samo | ✅ |
| Narzędziownia | 0xD598: 1 deska + 1 stal → narzędzie, 1535 + 3 × 1536; wybór wg priorytetów gracza | wybór: suma priorytetów >> 4 po każdym narzędziu osobno; oryginał przesuwa sumę narastającą (`(suma)>>4`) | ✅ (różnica zaokrąglenia, ułamek procenta) |
| Stocznia | 0xD4B8: 1 deska → łódź, 159 + 8 × 1408, czeka na wolną flagę | to samo | ✅ |
| Tartak | 0xE844: 1 pień → 1 deska, 2367 tików | to samo | ✅ |
| Kamieniarz | 0xE94C: 1536 tików, kamień 72→79 o jeden etap, ostatni znika, zawsze niesie kamień | to samo (port dodaje głazy z terraformowania – bez kamienia) | ✅ |
| Drwal | 0xFF94: 5 etapów, przewrócone drzewo / sosna | to samo (port dodaje martwe drzewa i palmy) | ✅ |
| **Leśniczy** | 0xFF1A: sosna lub drzewo liściaste wg **bitu 4 rejestru $DFF007** (pozycja promienia ekranu) | `random_int() & 1` – jedna liczba z generatora gry więcej | ⚠ zasada ta sama (50/50), inny przebieg losowania |
| Rolnik | 0xDAE8: siew na pustym polu bez drogi, zbiór: etapy pola 121–126, potem zużyte pole | to samo | ✅ |
| Złoto w budynkach wojskowych | 0xBCD6: tylko z rycerzem; do maks. (chata 2, wieża 4, twierdza 8), priorytet `((0xFE >> zapas)+1) & ~1` | to samo | ✅ |
| Morale rycerzy od złota | 0xA446: 1024 + współczynnik · złoto / całe złoto mapy (skalowanie > 65535, złoto < całość), zamki: −1023 / +1024 · wynik; współczynnik 10240 · liczba graczy (0x4E46) | to samo (oryginał dzieli najpierw `złoto·65536/całość`, różnica zaokrąglenia o 1) | ✅ |
| Place budowy – deski, kamień | 0xB052…: do potrzebnej liczby, deski: priorytet gracza >> zapas, kamień 0xFF >> zapas, bez budowniczego ÷ 4 | to samo (już wcześniej porównane, `update_unfinished`) | ✅ |
| Domyślne priorytety gracza | 0x15842: jedzenie 13100 / 45850 / 45850 / 65500, deski 65500 / 3275 / 19650, stal 45850 / 65500, węgiel 32750 / 65500 / 52400, zboże 65500 / 32750, narzędzia 9825, 65500, 13100, 6550, 13100, 26200, 32750, 45850, 6550 | to samo (`player.cc` `reset_*_priority`) | ✅ |

## Szkolenie rycerzy (sprawdzone 16.11.2026)

Rycerz awansuje o jeden poziom, gdy los (0–65535) < próg; próba co 6000 tików (60 s przy
normalnym tempie), po awansie licznik od nowa 6000. Złoto szkolenia nie przyspiesza (złoto działa
tylko na morale, wyżej). Poziom 4 już się nie szkoli.

| Gdzie | Oryginał (Amiga) | Freeserf | Zgodne? |
|---|---|---|---|
| magazyn / zamek, rycerz bezczynny (stan 1, 0x10186) | 4000 / 2000 / 1000 / 500 (poziom 0 / 1 / 2 / 3); szkoli się, chyba że magazyn wysyła osadników (tryb „out”) i kolejka < 3 – wtedy wychodzi | to samo (`handle_serf_idle_in_stock_state`) | ✅ |
| zamek – obrońca (stan 75, 0x10412) | 4000 / 2000 / 1000 / 500 | to samo | ✅ |
| twierdza (stan 72, 0x103FE) | 2000 / 1000 / 500 / 250 | to samo | ✅ |
| wieża (stan 71, 0x103EA) | 1000 / 500 / 250 / 125 | to samo | ✅ |
| chata (stan 70, 0x103D6) | 250 / 125 / 62 / 31 | to samo | ✅ |

Tablice progów w oryginale zachodzą na siebie (0x10426, słowa co 4 bajty: 4000, 2000, 1000, 500,
250, 125, 62, 31), pętla próby 0x1045E (`random < próg` → typ + 1, wynik wojskowy gracza + 1).

Średni czas awansu (gra w normalnym tempie): 65536 / próg minut.

| Awans | magazyn, zamek | twierdza | wieża | chata |
|---|---|---|---|---|
| 0 → 1 | 16 min | 33 min | 66 min | 4,4 h |
| 1 → 2 | 33 min | 66 min | 2,2 h | 8,7 h |
| 2 → 3 | 66 min | 2,2 h | 4,4 h | 17,6 h |
| 3 → 4 | 2,2 h | 4,4 h | 8,7 h | 35 h |
| 0 → 4 razem | **4,1 h** | 8,2 h | 16,4 h | 66 h |

## Walka rycerzy (sprawdzone 16.11.2026)

Oryginał 0xCDD8–0xCED2, Freeserf `Serf::set_fight_outcome` (`serf.cc`) – zgodne instrukcja po
instrukcji:

- siła = 1024 · 2^poziom · czynnik / 65536; czynnik = 4096 na własnej ziemi, na cudzej
  **morale gracza** (to od złota, wyżej);
- los = (siła napastnika + siła obrońcy) · random / 65536; **napastnik wygrywa, gdy los < jego
  siła**, czyli z szansą siła_n / (siła_n + siła_o);
- przegrany gracz traci 2^poziom punktów siły wojskowej i rycerza tego poziomu; początek animacji
  walki `random & 0x70`.

Przykład: obrońca w swoim budynku (czynnik 4096 = 1024 siły na poziomie 0), napastnik na jego
ziemi z morale 1024 (bez złota): poziom 0 na 0 – siła 256 na 1024, wygrywa w 20%; morale 4096 –
50%. Każdy poziom podwaja siłę.

## Zamiana osadników w rycerzy (sprawdzone 16.11.2026)

Oryginał 0x959C–0x962E (gracz z zamkiem), Freeserf `Player::update` – zgodne:

- co `(60 − rozmnażanie) · 50` tików (0x45C2; od 10 s do 30 s przy normalnym tempie) powstaje
  nowy osadnik w magazynie;
- licznik 16-bitowy += tempo „osadnicy → rycerze” (suwak gracza, domyślnie 20000, 0x453A);
  przy przepełnieniu +1 rycerz do zrobienia (najwyżej 2 w kolejce) – przy 20000 to ~31% nowych
  osadników;
- rycerz powstaje tylko, gdy w magazynie jest miecz i tarcza (zużywa oba); bez nich powstaje
  zwykły osadnik, a rycerz czeka w kolejce.

## Geolog (sprawdzone 16.11.2026)

Oryginał 0xD3DC (szukanie miejsca) i 0xD30C (badanie), Freeserf
`handle_serf_looking_for_geo_spot_state` / `handle_serf_sampling_geo_spot_state` – zgodne:

- 8 losowań miejsca w spirali 1–64 pól (`((los>>2)&63)+1`), tylko wolne pole z górami (tundra –
  śnieg 0, typy 11–14) w jednym z 4 trójkątów; dwa trafienia na stojący znaczek kończą szukanie;
- znaczek: pusty, gdy brak złoża; inaczej rodzaj złoża, **mały poniżej 12 jednostek**, duży od 12;
- komunikat „znaleziono …” tylko, gdy w 60 polach wokół nie ma znaczka tego samego surowca;
  64 tiki na znaczek.

## Transport i tragarze (sprawdzone 16.11.2026, tabele i główne decyzje)

| Co | Oryginał (Amiga) | Freeserf | Zgodne? |
|---|---|---|---|
| kategoria długości drogi | 0x156BC: progi 4, 6, 7, 10, 13, 18, 24 → 0–7 | `Flag::get_road_length_value` | ✅ |
| najwięcej tragarzy na drogę | tablica 1, 2, 3, 4, 6, 8, 11, 15 (0x25176, 0x260AA, 0x26772) | `max_transporters` w `Flag::update` | ✅ |
| wzywanie tragarza / „zatkana” flaga | 0x26042…: ≥ 7 towarów na fladze → droga bez wolnego tragarza, tragarz wołany, gdy brak wolnego lub czekają ≥ 3 towary w tym kierunku i jest mniej niż maks.; osobno ląd (0xA554) i woda (0xA55C) | `Flag::update` | ✅ |
| który towar tragarz bierze | najwyższy priorytet flagi gracza (porównanie w 0x25FE8) | `prioritize_pickup`, `schedule_known_dest_cb_` | ✅ |
| domyślne priorytety flag | 0x15706: ruda złota 1, złoto 2, zboże 3, mąka 4, świnia 5, łódź 6, obcęgi 7 … deska 26 | `reset_flag_priority` | ✅ |

Nie porównano instrukcja po instrukcji: przeszukiwania sieci dróg (`FlagSearch`), chodzenia
tragarzy po drodze i przekazywania towarów między flagami.

## Magazyny (sprawdzone 16.11.2026)

| Co | Oryginał (Amiga) | Freeserf | Zgodne? |
|---|---|---|---|
| kolejność wysyłania towarów do budynków | 0x9A42: los & 7 – 0: lista 2, 1: lista 3, inaczej lista 1 (tablice 0x9EB6, 0x9F00, 0x9E6C) | `Game::update_inventories`: te same trzy listy, to samo losowanie | ✅ |
| lista 1 (6 na 8) | deski, kamień, stal, węgiel, pnie, ruda żelaza, jedzenie, świnie, mąka, zboże, złoto, ruda złota | to samo | ✅ |
| lista 2 (1 na 8) | kamień, ruda żelaza, ruda złota, węgiel, stal, złoto, jedzenie, świnie, mąka, zboże, pnie, deski | to samo | ✅ |
| lista 3 (1 na 8) | jedzenie, zboże, świnie, mąka, złoto, kamień, deski, stal, węgiel, pnie, ruda złota, ruda żelaza | to samo | ✅ |
| magazyn w trybie „wysyłaj” | towar o najwyższym priorytecie magazynu gracza | to samo | ✅ |
| domyślne priorytety magazynu | 0x15706+: zboże 1, mąka 2, świnia 3, chleb 4, ryba 5, mięso 6 … złoto 26 | `reset_inventory_priority` | ✅ |

## Różnice, które zmieniają grę

### 1. Kopalnie zużywają w oryginale ok. 7 razy mniej jedzenia

Stan 0 procedury 0xDF56:

```
andi.w #7,d4     ; los & 7
bne    → stan 2  ; ≠ 0 (7 na 8): bez jedzenia
addq   → stan 1  ; = 0 (1 na 8): bierze porcję, gdy brak – czeka
```

Urobek na porcję (p – część z 32 pól wokół kopalni ze złożem, wejście udane z szansą 1−(1−p)⁴):

| Złoże (p) | Oryginał: 8·(1−(1−p)⁴) | Freeserf: (8/7)·(1−(1−p)⁴) |
|---|---|---|
| 1,0 | 8,0 | 1,14 |
| 0,5 | 7,5 | 1,07 |
| 0,25 | 5,5 | 0,78 |
| 0,1 | 2,8 | 0,39 |

Bez jedzenia kopalnia w oryginale kopie dalej, aż trafi wejście „z jedzeniem” (średnio po ~7),
wtedy staje. Czasy są takie same: ~27 s na urobek przy bogatym złożu, do ~57 s przy 4 próbach.

### 2. Rybacy łowią w oryginale dużo częściej

Ryb na polu wody jest 0–7 (`map-generator.cc`: `random_int() & 7`). Szansa jednej próby
i połowu w jednym wyjściu (do 10 prób):

| Ryb na polu | Oryginał: próba / wyjście | Freeserf: próba / wyjście |
|---|---|---|
| 1 | 7,8% / 56% | 0% / 0% |
| 3 | 10,9% / 69% | 0% / 0% |
| 5 | 14,1% / 78% | 1,6% / 15% |
| 7 | 17,2% / 85% | 4,7% / 38% |
| średnio (0–7) | – / **63%** | – / **10%** |

We Freeserf woda z 1–4 rybami nigdy nic nie daje; w oryginale każda woda z rybami daje.

### 3. Przyciski „szkól 1 / 5 / 20 / 100” w oknie rycerzy

Oryginał 0x16940: pętla po bezczynnych osadnikach w magazynach, każdy z mieczem i tarczą zostaje
rycerzem, licznik z przycisku −1, **przy zerze koniec** (`subq #1,d4; beq`). Freeserf
(`Player::promote_serfs_to_knights`, `player.cc`) zmniejsza licznik, ale się nie zatrzymuje – każdy
przycisk zamienia wszystkich możliwych osadników. To błąd Freeserf (zmienna `number` nic nie robi).
Opis okna: `docs/OKNO_RYCERZY.md`.

### 4. Obrońcy zamku

Zamek chce tylu obrońców, ile ustawiono w oknie rycerzy (16 / 19, domyślnie 3, od 1 do 99).
Oryginał 0xB824 (co krok budynku zamku), porównanie z `Building::update_castle` (`building.cc`):

| Sytuacja | Oryginał Amigi | Port (Freeserf) |
|---|---|---|
| obrońców mniej | z magazynu zamku najsilniejszy wolny rycerz (poziom 4 → 0); gdy brak – nowy rycerz z wolnego osadnika (miecz + tarcza); gdy i to nie – wezwanie rycerza z innego magazynu (co 5. krok) | to samo ✅ |
| obrońców więcej | pierwszy z listy wraca do magazynu zamku | to samo ✅ |
| obrońców tylu, ilu trzeba | **najsilniejszy obrońca zamienia się poziomem z najsłabszym wolnym rycerzem w magazynie zamku**, jeśli taki słabszy jest (0xB972–0xBA18: szuka maks. poziomu wśród obrońców, potem w magazynie poziomów 0, 1, 2, 3 poniżej niego, zamienia typy) | najsłabszy obrońca zamienia się poziomem z **ostatnim obrońcą** z listy; pętla `call_internal` nic nie robi (obrońca nie jest w magazynie) – **bez wymiany z magazynem** ❌ |

Skutek: w oryginale w zamku zostają najsłabsi rycerze, a silni czekają w magazynie zamku i idą do
budynków wojskowych i do ataku. W porcie silni rycerze, którzy weszli do obrony, zostają w zamku.
Szkolenie obrońców i czekających w magazynie zamku jest takie samo (4000 / 2000 / 1000 / 500).

## Czego nie sprawdzono

Przeszukiwanie sieci dróg i chodzenie tragarzy (instrukcja po instrukcji), budowanie (poza zamawianiem materiałów), komputerowy przeciwnik (port go już
przenosił z oryginału – adresy w `ai-*.cc`). Wersji DOS nie mamy – nie wiadomo, czy różnice 1 i 2
są w DOS, czy powstały przy przepisywaniu Freeserf (3 i 4 to błędy przepisania we Freeserf: licznik bez wpływu, zamiana z niewłaściwym rycerzem).

## Decyzja (17.11.2026)

Użytkownik: wszystkie 4 różnice **na stałe jak oryginał Amigi**, bez opcji – od wersji 0.138
(`serf.cc` kopalnie i rybak, `player.cc` przyciski szkól, `building.cc` obrońcy zamku;
`docs/PORTING.md`, „Zmiany 0.138”).

## Co z tym zrobić (pierwotny opis)

Różnice 1 i 2 to po jednej linii w `serf.cc`, 3 – jedna linia w `player.cc`, 4 – kilkanaście linii w `building.cc` (`update_castle`). Zmieniają przebieg gry: stare zapisy grają dalej
inaczej, w grze LAN obie strony muszą mieć tę samą wersję. Możliwości: na stałe jak oryginał
Amigi albo opcja (np. Advanced → „Zasady oryginału Amigi”), w grze LAN brana od serwera.
