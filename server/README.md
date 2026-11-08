# Settlers game server

Amigas running the Settlers port play over the internet through this server
(the INTERNET GAME button of the game's LAN box): one player makes a game and
gets a 4-letter code, the other picks it from the list or types the code; the
server passes the game's frames between them. It also keeps the bug reports
the game sends (F10) and shows a page with the games (home network only).

Python 3 (standard library only), one asyncio process; tested on a Raspberry
Pi 400 (Debian 13, Python 3.13), meant for a Raspberry Pi 4.

## Install (from the PC, in the repository)

```sh
sh tools/server_deploy.sh cizar@192.168.5.237
```

copies this folder to `~/settlers-server` on the Pi and starts the user
service `settlers-server` (and keeps it running after logout: linger).

By hand on the Pi:

```sh
mkdir -p ~/settlers-server && cp -r * ~/settlers-server/
mkdir -p ~/.config/systemd/user
cp settlers-server.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now settlers-server
sudo loginctl enable-linger $USER     # run without anyone logged in
```

## Router

Forward **TCP 6810** to the Pi. Do **not** forward 8080 (the page is for the
home network; it shows the e-mail addresses of bug reports).

## Ports and limits

`config.json`: game port 6810, page 8080, 50 games, 100 connections, 4 per
address, 30 minutes waiting for the second player, 60 s without data ends a
game, reports up to 4 MB, 20 per address a day, 1 GB in all.

## Data

`data/sessions.jsonl` (one line per game), `data/reports/<date>-<n>/`
(`report.json` and the files), `data/server.log`.

## Tests

```sh
python3 -m unittest discover -s tests -v
LOAD=1 python3 -m unittest tests.test_server.Load    # 50 games at once
```
