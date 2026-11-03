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
