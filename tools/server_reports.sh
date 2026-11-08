#!/bin/sh
# The bug reports of the game server to the PC: private/reports/ (not in git:
# e-mail addresses of players). Only the new folders are copied.
#   sh tools/server_reports.sh [cizar@192.168.5.237]
set -e
cd "$(dirname "$0")/.."
T=${1:-cizar@192.168.5.237}
mkdir -p private/reports
for d in $(ssh "$T" 'ls ~/settlers-server/data/reports 2>/dev/null'); do
  [ -d "private/reports/$d" ] && continue
  scp -q -r "$T:settlers-server/data/reports/$d" private/reports/
  echo "$d"
done
