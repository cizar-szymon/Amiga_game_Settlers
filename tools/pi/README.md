# Settlers na Raspberry Pi 400 (Amiberry)

Ten sam program co na Amidze (`Settlers`, `Settlers-LAN`) w emulatorze Amiberry. Dzięki temu
gra LAN Amiga ↔ Pi liczy się tak samo, bez rozjazdów: emulator wykonuje te same instrukcje 68k.

## 1. Karta SD (użytkownik)

1. Raspberry Pi Imager: urządzenie **Raspberry Pi 400**, system **Raspberry Pi OS (64-bit)**.
2. W ustawieniach przed zapisem: nazwa użytkownika i hasło, WiFi (lepiej kabel), **włączone SSH**.
3. Start Pi 400, adres IP (np. w routerze albo `hostname -I` na Pi).

## 2. Instalacja z PC (Claude albo użytkownik, Git Bash)

```sh
sh tools/pi/pi_deploy.sh uzytkownik@ADRES_PI            # bez autostartu
sh tools/pi/pi_deploy.sh uzytkownik@ADRES_PI autostart  # Amiberry startuje z pulpitem
```

Kopiuje przez `scp` (pyta o hasło SSH; lepiej klucz `ssh-copy-id`):

- ROM `C:\amiga-dev\roms\kicka1200.rom`;
- system z testów WinUAE `C:\amiga-dev\winuae\System` – OS 3.2 z AmiAgentem w User-Startup;
- grę z danymi do `~/amiga/Settlers`.

Potem na Pi `install-amiberry.sh` instaluje najnowszy Amiberry (paczka `.deb` ARM64 z GitHuba)
i zapisuje `~/amiga/Settlers-Pi400.uae`. ROM, system i dane gry nie trafiają do repozytorium.

Start ręczny na Pi: `amiberry -G -config=~/amiga/Settlers-Pi400.uae`.

## 3. Konfiguracja (`Settlers-Pi400.uae.in`)

| | |
|---|---|
| Procesor | 68040 z JIT (`cachesize=16384`). JIT Amiberry na ARM jest dla 020–040; gra działa na 040 |
| Pamięć | Chip 4 MB, Z3 Fast 256 MB |
| Obraz | uaegfx (Picasso96) Z3, 32 MB; gra otwiera własny ekran 8-bit |
| Sieć | `bsdsocket_emu=true` – gra LAN i AmiAgent przez sieć Pi |
| Dyski | `DH0:` system, `SETTLERS:` gra, `WOW:` pusty tylko do odczytu (bez niego wspólny User-Startup staje na prośbie o WOW:) |

## 4. Sprawdzenie

1. `python tools/amiga_remote.py --host ADRES_PI ping` – AmiAgent w emulowanej Amidze (hasło jak w
   `amiagent-winuae.cfg`, bo to ten sam system).
2. `run "SETTLERS:lantest search"` przy RUN SERVER na Amidze 2 – czy rozgłoszenia dochodzą przez
   bsdsocket Amiberry.
3. Gra: `SETTLERS:Settlers-LAN`, STATS – liczba klatek na Pi 400 przy 640×480.
