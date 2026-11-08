# Settlers – port Freeserf (The Settlers 1) na Amigę RTG 68040/68060

@C:/Workprojects/AmigaGAMES/PORTING_PLAN.md
@C:/Workprojects/AmigaGAMES/AmiAgent/AMIAGENT.md

Plan, decyzje i stan prac: `docs/PORTING.md`. Aktualizować go po każdej fazie.

- Baza: Freeserf (C++11, GPL-3.0), natywny backend Amigi bez SDL; ekran P96 8-bit CLUT 640×480 lub 640×400 (800×600 do pomiaru).
- Toolchain Amigi: bebbo amiga-gcc w Dockerze (wzór `UFO/docker/build.ps1`), `-mcpu=68020 -msoft-float -mtune=68060 -O1` (nie `-O2`, nie `-m68040`).
- Testujemy na Amidze (`tools/amiga_remote.py`, `WorkSD:Games/Settlers`); testów na PC już nie robimy.
- Budowa Amigi: `docker/build.ps1` (PowerShell) → `build/amiga/`.
- **Bez procesora ARM** karty Warp.
- Dane gry (ADF-y w `AmigaSettlersADF/`) nie trafiają do repozytorium ani do wydań.
- Katalog na Amidze: `WorkSD:Games/Settlers`, okno/ekran z tytułem zaczynającym się od „Settlers”.

## Zgłoszenia błędów od graczy (serwer gry na Pi)

Gracze wysyłają zgłoszenia z gry (F10 → okno zgłoszenia; rozjazd gry LAN → zapisy obu stron) na
serwer gry (Pi, `ssh cizar@192.168.5.237`, katalog `~/settlers-server`, opis w `server/CLAUDE.md`).
Gdy użytkownik prosi o sprawdzenie zgłoszeń / błędu gracza:

1. Pobrać nowe: `sh tools/server_reports.sh` → `private/reports/<rrrrmmdd-ggmmss>-<nr>/`
   (`report.json`: data, wersja, rodzaj `F10`/`LAN`, e-mail, opis „co się stało”, maszyna; pliki).
   Inaczej (tylko sieć domowa): lista `curl -s http://192.168.5.237:8080/reports.json`,
   jedno `curl -o r.zip http://192.168.5.237:8080/reports/<katalog>.zip`, wszystkie `/reports/all.zip`.
2. `F10`: `Settlers-state.save` (pełny zapis gry) + `Settlers-state.txt` (wersja, tick, widok `VIEW=`).
   Odtworzenie na Amidze: zapis do `WorkSD:Games/Settlers-test/`, start `Settlers LOAD=<plik>`
   (przy wersji z `report.json`).
3. `LAN`: `SYNC_…M/S.lan.save` i `.txt` obu stron z tego samego kroku – `python tools/lan_diff.py`.
4. Po poprawce: pole `status` w `report.json` na Pi (np. `fixed in 0.132`), wpis w `docs/PORTING.md`.
   **E-maile i IP graczy nie trafiają do repo, commitów ani dokumentów** – zostają w `private/` i na Pi.
