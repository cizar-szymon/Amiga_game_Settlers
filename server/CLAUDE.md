# Settlers – serwer gry przez internet (Pi400 / RPi4)

Ten katalog (`~/settlers-server` na Pi) to **jedyne miejsce rzeczy serwera**. Nic poza nim nie
tworzyć ani nie zmieniać (poza usługą systemd użytkownika, niżej). Źródło jest w repo portu
Settlers na PC (`server/`), wgrywane przez `tools/server_deploy.sh` – zmiany kodu robić tam, nie tu.
Wyjątek: pilna poprawka na Pi – potem przenieść ją do repo.

## Co robi

- **Port 6810 (TCP, przekierowany na routerze z 83.12.152.135):** Amigi grają przez internet
  (okno LAN gry → INTERNET GAME). Jedna Amiga zakłada grę (`CREATE` → kod 4 znaki, opcjonalnie
  hasło), druga dołącza (`LIST` / `JOIN` kodu, z hasłem, jeśli gra je ma); serwer przekazuje ramki gry w obie strony i mierzy jakość
  połączenia (PING/PONG). Protokół: `protocol.py`, gra: `port/amiga_net.c` w repo.
- **Zgłoszenia błędów** z gry (`REPORT`): `data/reports/<rrrrmmdd-ggmmss>-<nr>/report.json`
  (data, godzina, wersja, rodzaj, **e-mail gracza**, opis, maszyna, adres, pliki) + pliki.
  Rodzaje: `F10` – gracz nacisnął F10 (`Settlers-state.save` – pełny zapis gry, `.txt` – wersja,
  tick, ekran, widok `VIEW=`); `LAN` – gry LAN rozjechały się (`SYNC_…M/S.lan.save` – zapisy
  obu stron z tego samego kroku, `SYNC_…M/S.txt` – sumy i ostatnie polecenia; porównanie:
  `tools/lan_diff.py` w repo). Pole `status` (`new` → np. `fixed in 0.132`) zmieniać ręcznie
  po poprawce; e-mail służy do powiadomienia gracza.
- **Strona** `http://<pi>:8080` (tylko sieć domowa, port **nie** przekierowany): gry teraz,
  czekające, ostatnie gry (gracze, IP, ping), zgłoszenia. Podstrona **`/load`** (przycisk Load u góry):
  bloki „Raspberry Pi” (CPU, sieć) i „Settlers game server” (CPU procesu, dane gier od / do Amig) –
  teraz, najwyżej w ostatniej godzinie, wykres godziny; `/load.json`. Inny serwer na tym Pi dopisuje
  swój blok (`Web.load_blocks`, `PAGES` w `web.py`). `/stats.json`, `/reports.json`;
  zgłoszenie: `/reports/<katalog>/` (szczegóły i pliki), `/reports/<katalog>.zip`, wszystkie
  `/reports/all.zip`; przycisk **Delete** (żarty, puste zgłoszenia) przenosi zgłoszenie do
  `data/deleted/` – znika ze strony, na dysku zostaje do ręcznego wyczyszczenia.
- Historia gier: `data/sessions.jsonl`; log: `data/server.log` (rotacja 5 × 2 MB).

## Pliki

| Plik | Do czego |
|---|---|
| `settlers_server.py` | start, konfiguracja, log |
| `relay.py` | sesje, parowanie, przekazywanie, limity, pomiar |
| `reports.py` | przyjmowanie i zapis zgłoszeń |
| `web.py` | strona i JSON |
| `load.py` | obciążenie co 5 s (CPU Pi i serwera, dane gier, sieć Pi; teraz i maks. z godziny) |
| `config.json` | porty, limity (50 gier, 100 połączeń, 4 na adres, czasy), dozwolone sieci strony |
| `tests/test_server.py` | testy z fałszywymi Amigami |
| `settlers-server.service` | usługa systemd użytkownika |

## Polecenia

```sh
systemctl --user status settlers-server        # stan
systemctl --user restart settlers-server       # po zmianie plików / config.json
journalctl --user -u settlers-server -n 50     # wyjście usługi
tail -f data/server.log                        # log serwera
python3 -m unittest discover -s tests          # testy (porty losowe, nie przeszkadzają usłudze)
LOAD=1 python3 -m unittest tests.test_server.Load   # 50 gier naraz przez ~20 s
curl -s localhost:8080/stats.json              # stan w JSON
ls data/reports/                               # zgłoszenia
```

## Zasady

- **Prywatność:** e-maile i adresy IP graczy tylko w `data/` i na stronie w sieci domowej.
  Nie wysyłać ich nigdzie, nie wpisywać do repo ani do commitów.
- Tylko Python 3 ze standardowej biblioteki (bez pip, bez sudo).
- Nie zmieniać portu 6810 bez zmiany w grze (domyślny adres `83.12.152.135:6810`, `Settlers.ini [lan] relay`).
- Nie kasować `data/` (historia i zgłoszenia); stare zgłoszenia można przenieść do `data/old/`.
- Serwer na Pi400 to próba; docelowo RPi4 – ten sam katalog i usługa.
