#!/bin/sh
# The game server (server/) to the Pi: ~/settlers-server, the user service
# settlers-server started again. data/ on the Pi (history, reports) stays.
#   sh tools/server_deploy.sh [cizar@192.168.5.237] [--force]
# A restart cuts the games played over the server: it refuses while a game
# is on or a player waits (--force: restart anyway).
set -e
cd "$(dirname "$0")/.."
T=cizar@192.168.5.237
FORCE=
for a in "$@"; do
  case "$a" in --force) FORCE=1 ;; *) T=$a ;; esac
done
if [ -z "$FORCE" ]; then
  N=$(ssh "$T" 'curl -s -m 5 localhost:8080/stats.json' |
      python -c 'import json,sys
try: print(len(json.load(sys.stdin)["sessions"]))
except Exception: print(0)')
  if [ "$N" != "0" ]; then
    echo "$N game(s) on the server now (playing or waiting) - not restarted."
    echo "Later, or: sh tools/server_deploy.sh --force"
    exit 1
  fi
fi
ssh "$T" 'mkdir -p ~/settlers-server/tests ~/.config/systemd/user'
scp -q server/*.py server/config.json server/CLAUDE.md server/README.md \
    server/settlers-server.service "$T":settlers-server/
scp -q server/tests/*.py "$T":settlers-server/tests/
ssh "$T" "FORCE=$FORCE"' ; cp ~/settlers-server/settlers-server.service ~/.config/systemd/user/ &&
  systemctl --user daemon-reload &&
  systemctl --user enable settlers-server >/dev/null 2>&1 &&
  { [ -n "$FORCE" ] || [ "$(curl -s -m 5 localhost:8080/stats.json |
      python3 -c "import json,sys; print(len(json.load(sys.stdin)[sys.argv[1]]))" sessions 2>/dev/null)" = 0 ] ||
    { echo "a game came meanwhile - files copied, not restarted"; exit 1; }; } &&
  systemctl --user restart settlers-server && sleep 2 &&
  systemctl --user --no-pager status settlers-server | head -5;
  loginctl show-user "$USER" -p Linger'
