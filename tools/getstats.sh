#!/bin/sh
# Prints RAM:Settlers-stats.txt from the Amiga (the game writes it every
# second with STATS / STATS=FULL). Retries when it is just being replaced.
# usage: sh tools/getstats.sh [amiagent config]
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
for i in 1 2 3 4 5; do
  out=$(python tools/amiga_remote.py --config "$CFG" run "Type RAM:Settlers-stats.txt" 2>/dev/null)
  case "$out" in
    *FPS*) echo "$out" | grep -v '^\[rc'; exit 0 ;;
  esac
  sleep 1
done
echo "no stats"
exit 1
